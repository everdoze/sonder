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
    };

    void prepare (double sampleRate, int maxBlockSize)
    {
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
        }
    }

    // modulation: сдвиги нормированных значений ручек, kNumFxParams на слот (nullptr - без модуляции)
    void process (float* left, float* right, int numSamples, const FxRack& rack,
                  const ParamRefs& params, const OnRefs& on, double bpm, const float* modulation = nullptr) noexcept
    {
        for (int slotIndex : rack.getOrder())
        {
            auto& slot = slots[(size_t) slotIndex];
            const auto type = rack.getType (slotIndex);
            const bool isOn = type != FxType::none && on[(size_t) slotIndex]->load() > 0.5f;

            // Слот только что включили или сменили в нём эффект: чистим "хвосты" прошлого
            if (isOn && (type != slot.lastType || ! slot.wasOn))
                reset (slot, type);

            slot.lastType = type;
            slot.wasOn = isOn;

            if (! isOn)
                continue;

            // Значения ручек из нормированных в реальные единицы
            const auto& info = getFxTypeInfo (type);
            std::array<float, kNumFxParams> values {};
            for (size_t i = 0; i < info.params.size(); ++i)
            {
                float normalised = params[(size_t) slotIndex][i]->load();

                // Списки (режим, синхронизация) не модулируются: скачки между вариантами - это не модуляция
                if (modulation != nullptr && ! info.params[i].isChoice())
                    normalised = std::clamp (normalised + modulation[(size_t) slotIndex * kNumFxParams + i], 0.0f, 1.0f);

                values[i] = info.params[i].toReal (normalised);
            }

            const float* p = values.data();

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
    }

    // Для визуализации
    const Slot& getSlot (int index) const noexcept { return slots[(size_t) index]; }

private:
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
};

} // namespace sonder
