#pragma once

#include "Saturation.h"

#include <array>
#include <cmath>

namespace sonder
{

// Дисторшн на один канал: в голосе (до или после фильтра) и в рэке эффектов.
//
// Tube - модель лампового предусилителя, а не одна кривая:
//  - разделительный конденсатор на входе срезает низ перед усилением (чем больше драйв, тем выше срез),
//    иначе бас на перегрузе "пердит" и забивает всё остальное;
//  - два инвертирующих триодных каскада, усиление разнесено между ними. Триод ограничивает несимметрично:
//    вверх мягче и дальше (сеточный ток), вниз раньше (отсечка) - отсюда чётные гармоники и "тёплый" характер;
//  - сеточный ток заряжает разделительный конденсатор, рабочая точка съезжает от громкого сигнала:
//    атака чуть сжимается и "расцветает", как у настоящего усилителя;
//  - между каскадами ёмкость Миллера заваливает верх, на выходе мягкий "оконечный" каскад и срез верхов,
//    который опускается с ростом драйва - на сильном перегрузе нет песочного шипения.
// Все кривые гладкие (без изломов и обрезки): меньше гармоник выше половины частоты дискретизации,
// меньше алиасинга и "грязи", которая копится, если поставить несколько дисторшнов подряд.
class Distortion
{
public:
    // Порядок совпадает с Choices::distTypes()
    enum class Type { off, tube, hard, fold, crush };

    static constexpr int kTubeStages = 2;

    void prepare (double sampleRate) noexcept
    {
        rate = (float) sampleRate;
        dcCoef = onePoleCoef (20.0f);
        millerCoef = onePoleCoef (12000.0f);
        couplingA = std::exp (-6.2831853f * 12.0f / rate);
        biasAttack = 1.0f - std::exp (-1.0f / (0.002f * rate));
        biasRelease = 1.0f - std::exp (-1.0f / (0.08f * rate));
        cachedDrive = -1.0f;
        reset();
    }

    void reset() noexcept
    {
        dcState = toneState = held = 0.0f;
        holdCounter = 0;
        inputState = inputPrevious = outputState = 0.0f;
        stages = {};
    }

    struct Settings
    {
        Type type = Type::off;
        float drive = 0.0f, gain = 1.0f, compensation = 1.0f, mix = 0.0f, toneCoef = 1.0f;
        float crushLevels = 4096.0f;
        int crushHold = 1;
        float stageGain[kTubeStages] { 1.0f, 1.0f };
    };

    // drive 0..1, mix 0..1, toneCoef - коэффициент ФНЧ после искажения (1 = без фильтра)
    static Settings makeSettings (Type type, float drive, float mix, float toneCoef) noexcept
    {
        Settings s;
        s.type = type;
        s.drive = drive;
        s.mix = mix;
        s.toneCoef = toneCoef;
        s.crushLevels = std::exp2 (12.0f - 10.0f * drive);
        s.crushHold = 1 + (int) (drive * drive * 24.0f);

        if (type == Type::tube)
        {
            // Первый каскад набирает усиление быстрее, второй добавляет плотности на верхней половине ручки
            s.stageGain[0] = 1.0f + 9.0f * drive * drive;
            s.stageGain[1] = 1.0f + 2.0f * drive;
            s.gain = s.stageGain[0] * s.stageGain[1];
            s.compensation = 1.15f * std::pow (s.gain, -0.22f);
        }
        else
        {
            s.gain = 1.0f + 29.0f * drive * drive;
            // Примерная компенсация громкости: сильнее драйв - тише, чтобы звук не "взрывался"
            s.compensation = std::pow (s.gain, -0.275f);
        }

        return s;
    }

    // Статическая кривая искажения без памяти (её рисует интерфейс; у Tube - без фильтров и сдвига смещения)
    static float shape (float x, const Settings& s) noexcept
    {
        switch (s.type)
        {
            case Type::tube:
            {
                float u = x;
                for (int stage = 0; stage < kTubeStages; ++stage)
                    u = -(triode (s.stageGain[stage] * u + kBias[stage]) - triode (kBias[stage]));

                return smoothClip (u * kOutputDrive) * s.compensation;
            }

            case Type::hard:  return std::fmin (std::fmax (x * s.gain, -1.0f), 1.0f) * s.compensation;
            case Type::fold:  return fastSinCycles (0.25f * x * s.gain) * s.compensation;
            case Type::crush: return std::round (x * s.crushLevels) / s.crushLevels;
            case Type::off:   break;
        }

        return x;
    }

    float process (float x, const Settings& s) noexcept
    {
        if (s.type == Type::off || s.mix <= 0.0f)
            return x;

        float y = 0.0f;

        if (s.type == Type::tube)
        {
            y = processTube (x, s);
        }
        else if (s.type == Type::crush)
        {
            // Кроме разрядности падает и частота дискретизации: значение держится несколько сэмплов
            if (--holdCounter <= 0)
            {
                holdCounter = s.crushHold;
                held = shape (x, s);
            }

            y = held;
        }
        else
        {
            y = shape (x, s);
        }

        // Асимметричные кривые рождают постоянную составляющую - убираем
        dcState += (y - dcState) * dcCoef;
        y -= dcState;

        toneState += (y - toneState) * s.toneCoef;
        y = toneState;

        return x + (y - x) * s.mix;
    }

private:
    // Рабочие точки каскадов и "раскачка" оконечного каскада
    static constexpr float kBias[kTubeStages] { 0.22f, 0.12f };
    static constexpr float kOutputDrive = 0.9f;

    // Гладкая сигмоида x / sqrt(1 + x^2): в отличие от приближённого tanh нигде не обрезается
    static float smoothClip (float x) noexcept
    {
        return x / std::sqrt (1.0f + x * x);
    }

    // Триод: несимметричное ограничение - вверх до 1.15 (сеточный ток), вниз только до -0.85 (отсечка).
    // Кривая гладкая, без излома в нуле: излом давал бы лишние высокие гармоники и алиасинг.
    static float triode (float u) noexcept
    {
        constexpr float asymmetry = 0.15f;
        const float s = smoothClip (u);
        return s + asymmetry * s * s;
    }

    float onePoleCoef (float frequency) const noexcept
    {
        return 1.0f - std::exp (-6.2831853f * std::fmin (frequency, 0.45f * rate) / rate);
    }

    float processTube (float x, const Settings& s) noexcept
    {
        // Коэффициенты, которые зависят от драйва, пересчитываем только при его изменении
        if (s.drive != cachedDrive)
        {
            cachedDrive = s.drive;
            inputA = std::exp (-6.2831853f * (15.0f + 110.0f * s.drive * s.drive) / rate);
            outputCoef = onePoleCoef (18000.0f * std::exp2 (-1.4f * s.drive));
        }

        // Разделительный конденсатор на входе
        inputState = inputA * (inputState + x - inputPrevious);
        inputPrevious = x;
        float u = inputState;

        for (int i = 0; i < kTubeStages; ++i)
        {
            auto& stage = stages[(size_t) i];

            // Смещение, которое сдвинул сеточный ток
            const float bias = kBias[i] - stage.biasShift;
            const float grid = s.stageGain[i] * u + bias;
            const float y = -(triode (grid) - triode (bias));

            const float charge = grid > 0.3f ? (grid - 0.3f) * 0.35f : 0.0f;
            stage.biasShift += (charge - stage.biasShift) * (charge > stage.biasShift ? biasAttack : biasRelease);

            // Ёмкость Миллера и разделительный конденсатор до следующего каскада
            stage.miller += (y - stage.miller) * millerCoef;
            stage.coupling = couplingA * (stage.coupling + stage.miller - stage.couplingPrevious);
            stage.couplingPrevious = stage.miller;
            u = stage.coupling;
        }

        // Оконечный каскад и завал верха
        const float out = smoothClip (u * kOutputDrive) * s.compensation;
        outputState += (out - outputState) * outputCoef;
        return outputState;
    }

    struct Stage
    {
        float miller = 0.0f, coupling = 0.0f, couplingPrevious = 0.0f, biasShift = 0.0f;
    };

    float rate = 44100.0f;
    float dcCoef = 0.001f;
    float dcState = 0.0f, toneState = 0.0f, held = 0.0f;
    int holdCounter = 0;

    float millerCoef = 1.0f, couplingA = 1.0f, biasAttack = 1.0f, biasRelease = 1.0f;
    float cachedDrive = -1.0f, inputA = 1.0f, outputCoef = 1.0f;
    float inputState = 0.0f, inputPrevious = 0.0f, outputState = 0.0f;
    std::array<Stage, kTubeStages> stages {};
};

} // namespace sonder
