#pragma once

#include "Chorus.h"
#include "Compressor.h"
#include "Equalizer.h"
#include "FilterFx.h"
#include "Flanger.h"
#include "FreqShifter.h"
#include "FxRack.h"
#include "Limiter.h"
#include "MasterDistortion.h"
#include "Multiband.h"
#include "Phaser.h"
#include "Reverb.h"
#include "TapeDelay.h"
#include "Tremolo.h"
#include "Widener.h"

namespace sonder
{

// Обработка рэка в аудиопотоке. У каждого слота заранее созданы все виды эффектов,
// поэтому смена вида в слоте не требует выделения памяти.
class FxChain
{
public:
    using ParamRefs = std::array<std::array<std::atomic<float>*, kNumFxParams>, kNumFxSlots>;
    using OnRefs = std::array<std::atomic<float>*, kNumFxSlots>;

    struct Slot
    {
        MasterDistortion distortion;
        Phaser phaser;
        Flanger flanger;
        Chorus chorus;
        TapeDelay delay;
        Compressor compressor;
        Reverb reverb;
        Equalizer equalizer;
        FilterFx filter;
        Tremolo tremolo;
        Widener widener;
        Limiter limiter;
        Multiband multiband;
        FreqShifter shifter;

        FxType lastType = FxType::none;
        bool wasOn = false;

        // Сглаженные нормированные значения ручек: поворот ручки не даёт ступеньку (щелчок) раз в блок
        std::array<float, kNumFxParams> smoothed {};
        bool smoothedReady = false;
    };

    void prepare (double sampleRate, int maxBlockSize)
    {
        rate = (float) sampleRate;

        for (auto& slot : slots)
        {
            slot.distortion.prepare (sampleRate, maxBlockSize);
            slot.phaser.prepare (sampleRate);
            slot.flanger.prepare (sampleRate);
            slot.chorus.prepare (sampleRate);
            slot.delay.prepare (sampleRate);
            slot.compressor.prepare (sampleRate);
            slot.reverb.prepare (sampleRate);
            slot.equalizer.prepare (sampleRate);
            slot.filter.prepare (sampleRate);
            slot.tremolo.prepare (sampleRate);
            slot.widener.prepare (sampleRate);
            slot.limiter.prepare (sampleRate);
            slot.multiband.prepare (sampleRate);
            slot.shifter.prepare (sampleRate);
            slot.lastType = FxType::none;
            slot.wasOn = false;
            slot.smoothedReady = false;
        }
    }

    // modulation: сдвиги нормированных значений ручек, kNumFxParams на слот (nullptr - без модуляции)
    void process (float* left, float* right, int numSamples, const FxRack& rack,
                  const ParamRefs& params, const OnRefs& on, double bpm, const float* modulation = nullptr) noexcept
    {
        // Цели ручек и включение слотов на этот блок
        std::array<std::array<float, kNumFxParams>, kNumFxSlots> targets {};
        std::array<bool, kNumFxSlots> active {};
        bool moving = false;

        for (int slotIndex : rack.getOrder())
        {
            auto& slot = slots[(size_t) slotIndex];
            const auto type = rack.getType (slotIndex);
            const bool isOn = type != FxType::none && on[(size_t) slotIndex]->load() > 0.5f;

            // Слот только что включили или сменили в нём эффект: чистим "хвосты" прошлого,
            // ручки сразу встают на место
            if (isOn && (type != slot.lastType || ! slot.wasOn))
            {
                reset (slot, type);
                slot.smoothedReady = false;
            }

            slot.lastType = type;
            slot.wasOn = isOn;
            active[(size_t) slotIndex] = isOn;

            if (! isOn)
                continue;

            const auto& info = getFxTypeInfo (type);
            auto& target = targets[(size_t) slotIndex];

            for (size_t i = 0; i < info.params.size(); ++i)
            {
                float normalised = params[(size_t) slotIndex][i]->load();

                // Списки (режим, синхронизация) не модулируются и не сглаживаются: скачки между вариантами - это не модуляция
                if (modulation != nullptr && ! info.params[i].isChoice())
                    normalised = std::clamp (normalised + modulation[(size_t) slotIndex * kNumFxParams + i], 0.0f, 1.0f);

                target[i] = normalised;

                if (! slot.smoothedReady || info.params[i].isChoice())
                    slot.smoothed[i] = normalised;
                else
                    moving |= std::abs (slot.smoothed[i] - normalised) > 1.0e-5f;
            }

            slot.smoothedReady = true;
        }

        // Ручки на месте - блок целиком. Ручку крутят - кусками по 32 сэмпла, и значение в каждом куске
        // подъезжает к цели (около 10 мс), поэтому ни фильтр, ни громкость не прыгают ступенькой.
        const int chunk = moving ? 32 : numSamples;

        for (int start = 0; start < numSamples; start += chunk)
        {
            const int length = std::min (chunk, numSamples - start);
            const float coefficient = moving ? 1.0f - std::exp (-(float) length / (0.01f * rate)) : 1.0f;

            for (int slotIndex : rack.getOrder())
            {
                if (! active[(size_t) slotIndex])
                    continue;

                auto& slot = slots[(size_t) slotIndex];
                const auto type = rack.getType (slotIndex);
                const auto& info = getFxTypeInfo (type);
                const auto& target = targets[(size_t) slotIndex];

                std::array<float, kNumFxParams> values {};
                for (size_t i = 0; i < info.params.size(); ++i)
                {
                    auto& value = slot.smoothed[i];
                    value += (target[i] - value) * coefficient;
                    if (std::abs (target[i] - value) < 1.0e-5f)
                        value = target[i];

                    values[i] = info.params[i].toReal (value);
                }

                processSlot (slot, type, left + start, right + start, length, values.data(), bpm);
            }
        }
    }

    // Для визуализации
    const Slot& getSlot (int index) const noexcept { return slots[(size_t) index]; }

private:
    static void processSlot (Slot& slot, FxType type, float* left, float* right, int numSamples, const float* p, double bpm) noexcept
    {
        switch (type)
        {
            case FxType::distortion: slot.distortion.process (left, right, numSamples, p); break;
            case FxType::phaser:     slot.phaser.process (left, right, numSamples, p); break;
            case FxType::flanger:    slot.flanger.process (left, right, numSamples, p); break;
            case FxType::chorus:     slot.chorus.process (left, right, numSamples, p); break;
            case FxType::delay:      slot.delay.process (left, right, numSamples, p, bpm); break;
            case FxType::compressor: slot.compressor.process (left, right, numSamples, p); break;
            case FxType::reverb:     slot.reverb.process (left, right, numSamples, p); break;
            case FxType::equalizer:  slot.equalizer.process (left, right, numSamples, p); break;
            case FxType::filter:     slot.filter.process (left, right, numSamples, p, bpm); break;
            case FxType::tremolo:    slot.tremolo.process (left, right, numSamples, p, bpm); break;
            case FxType::widener:    slot.widener.process (left, right, numSamples, p); break;
            case FxType::limiter:    slot.limiter.process (left, right, numSamples, p); break;
            case FxType::multiband:  slot.multiband.process (left, right, numSamples, p); break;
            case FxType::shifter:    slot.shifter.process (left, right, numSamples, p); break;
            case FxType::none:
            case FxType::count:      break;
        }
    }

    static void reset (Slot& slot, FxType type) noexcept
    {
        switch (type)
        {
            case FxType::distortion: slot.distortion.reset(); break;
            case FxType::phaser:     slot.phaser.reset(); break;
            case FxType::flanger:    slot.flanger.reset(); break;
            case FxType::chorus:     slot.chorus.reset(); break;
            case FxType::delay:      slot.delay.reset(); break;
            case FxType::compressor: slot.compressor.reset(); break;
            case FxType::reverb:     slot.reverb.reset(); break;
            case FxType::equalizer:  slot.equalizer.reset(); break;
            case FxType::filter:     slot.filter.reset(); break;
            case FxType::tremolo:    slot.tremolo.reset(); break;
            case FxType::widener:    slot.widener.reset(); break;
            case FxType::limiter:    slot.limiter.reset(); break;
            case FxType::multiband:  slot.multiband.reset(); break;
            case FxType::shifter:    slot.shifter.reset(); break;
            case FxType::none:
            case FxType::count:      break;
        }
    }

    std::array<Slot, kNumFxSlots> slots;
    float rate = 44100.0f;
};

} // namespace sonder
