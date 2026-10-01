#pragma once

#include <algorithm>
#include <cmath>

namespace sonder::ScopeTrigger
{

// Где начинать рисовать волну в режиме WAVE, чтобы от кадра к кадру она стояла на месте.
struct Result
{
    float trigger = 0.0f;  // дробная позиция начала окна в буфере
    float period = 0.0f;   // настоящий период в сэмплах
    bool found = false;
};

// Нормированная автокорреляция на задержке lag (length отсчётов, через step)
inline double correlationAt (const float* x, int length, int lag, int step)
{
    double sum = 0.0, energyA = 0.0, energyB = 0.0;
    for (int i = 0; i < length; i += step)
    {
        const double a = x[i], b = x[i + lag];
        sum += a * b;
        energyA += a * a;
        energyB += b * b;
    }
    return energyA > 0.0 && energyB > 0.0 ? sum / std::sqrt (energyA * energyB) : 0.0;
}

// Период около nominal: максимум нормированной автокорреляции в пределах ±15%. Реальная высота отличается
// от номинальной (дрейф, бенд, глайд, строй), а октавных ошибок в таком узком диапазоне не бывает.
// value - насколько волна похожа на себя через найденный период (-1..1). 0 - посчитать нельзя.
inline float measureNearPeriod (const float* x, int total, float nominal, double& value)
{
    value = -1.0;
    const int minLag = std::max (2, (int) std::floor (nominal * 0.85f));
    const int maxLag = (int) std::ceil (nominal * 1.15f);
    const int length = std::min (total - maxLag - 1, (int) (nominal * 2.0f));

    if (length < 16 || maxLag >= total)
        return 0.0f;

    // Прореживание для низких нот и длинных периодов: точность почти не страдает, а считать в разы меньше
    const int step = std::max (1, length / 1024);
    const int lagStep = std::max (1, (int) (nominal / 512.0f));
    const auto correlation = [&] (int lag) { return correlationAt (x, length, lag, step); };

    int best = minLag;
    double bestValue = -2.0;
    for (int lag = minLag; lag <= maxLag; lag += lagStep)
    {
        const double current = correlation (lag);
        if (current > bestValue)
        {
            bestValue = current;
            best = lag;
        }
    }

    // Уточнение вокруг грубого максимума - по каждому сэмплу
    for (int lag = std::max (minLag, best - lagStep + 1); lag <= std::min (maxLag, best + lagStep - 1); ++lag)
    {
        const double current = correlation (lag);
        if (current > bestValue)
        {
            bestValue = current;
            best = lag;
        }
    }

    value = bestValue;

    // Дробная часть: парабола по трём соседним точкам
    if (best > minLag && best < maxLag)
    {
        const double left = correlation (best - 1), right = correlation (best + 1);
        const double denominator = left - 2.0 * bestValue + right;
        if (std::abs (denominator) > 1.0e-12)
            return (float) best + (float) std::clamp (0.5 * (left - right) / denominator, -0.5, 0.5);
    }

    return (float) best;
}

// Период, через который волна действительно повторяется. Суб на октаву ниже (или осциллятор на октаву-две
// ниже ноты) делает соседние периоды ноты разными: тогда настоящий период - два, три или четыре периода ноты.
// Берётся самый короткий из хорошо совпадающих: биения расстроенных осцилляторов кратный период не дают.
// 0 - период не найден.
inline float measurePeriod (const float* x, int total, float nominal)
{
    float periods[5] {};
    double values[5] { -1.0, -1.0, -1.0, -1.0, -1.0 };
    double bestValue = -1.0;

    for (int multiple = 1; multiple <= 4; ++multiple)
    {
        periods[multiple] = measureNearPeriod (x, total, nominal * (float) multiple, values[multiple]);
        if (periods[multiple] > 0.0f)
            bestValue = std::max (bestValue, values[multiple]);
    }

    if (bestValue < 0.5)
        return 0.0f;

    for (int multiple = 1; multiple <= 4; ++multiple)
        if (periods[multiple] > 0.0f && values[multiple] >= bestValue - 0.05)
            return periods[multiple];

    return 0.0f;
}

// Выравнивание по фазе основной гармоники: окно начинается там, где основной тон проходит через ноль вверх.
// В отличие от перехода через ноль самой волны, этот момент единственный за период и сдвигается плавно,
// даже если форма сложная (несколько переходов за период) и меняется от биений расстроенных осцилляторов.
// search - зона поиска начала от начала буфера; window - сколько сэмплов нужно после начала.
// period - настоящий период (measurePeriod) или номинальный, если измерить не удалось.
inline Result byFundamental (const float* x, int total, int search, float period, int window)
{
    Result result;

    // Самое позднее подходящее начало: свежая картинка, и окно помещается в буфер
    const double latest = std::min ((double) search - 1.0, (double) (total - window - 2));

    // Два периода перед этим местом с окном Ханна: фаза основной гармоники относительно их начала
    const int length = std::min (total, (int) std::round (period * 2.0f));
    if (length < 8 || latest < 0.0)
        return result;

    const int origin = std::max (0, std::min ((int) latest - length, total - length));
    x += origin;

    const double omega = 2.0 * 3.14159265358979 / period;
    double re = 0.0, im = 0.0, weights = 0.0, energy = 0.0;

    for (int n = 0; n < length; ++n)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * n / (length - 1));
        const double v = x[n] * w;
        re += v * std::cos (omega * n);
        im -= v * std::sin (omega * n);
        weights += w;
        energy += (double) x[n] * x[n];
    }

    // Основной тон слишком слаб (например, звук без основной гармоники) - пусть решает другой способ
    const double amplitude = 2.0 * std::sqrt (re * re + im * im) / weights;
    const double rms = std::sqrt (energy / length);
    if (amplitude < 0.2 * rms)
        return result;

    // x ~ A cos (omega n + phi); вверх через ноль при omega n + phi = -pi/2
    const double phi = std::atan2 (im, re);
    double start = (-0.5 * 3.14159265358979 - phi) / omega;
    start -= std::floor (start / period) * period;
    start += origin;

    if (latest < start)
        return result;

    start += std::floor ((latest - start) / period) * period;

    result.trigger = (float) start;
    result.period = period;
    result.found = true;
    return result;
}

// Один период картинки из points точек, начиная с дробной позиции start
inline void sampleShape (const float* x, float start, float period, float* shape, int points)
{
    for (int i = 0; i < points; ++i)
    {
        const float position = start + period * (float) i / (float) points;
        const int index = (int) position;
        const float frac = position - (float) index;
        shape[i] = x[index] + (x[index + 1] - x[index]) * frac;
    }
}

// Выравнивание по прошлому кадру: из начал в пределах одного периода (самых поздних) берём то, где первый
// период волны больше всего похож на нарисованный в прошлый раз. Форма от биений меняется плавно - и картинка
// меняется плавно, без скачков между разными точками периода. previous - points точек прошлого кадра.
inline Result byContinuity (const float* x, int total, int search, float period, int window,
                            const float* previous, int points)
{
    Result result;
    const double latest = std::min ((double) search - 1.0, (double) (total - window - 2));
    const double earliest = std::max (0.0, latest - (double) period);
    if (latest <= earliest || points < 8)
        return result;

    double previousMean = 0.0, previousEnergy = 0.0;
    for (int i = 0; i < points; ++i)
        previousMean += previous[i];
    previousMean /= points;
    for (int i = 0; i < points; ++i)
        previousEnergy += (previous[i] - previousMean) * (previous[i] - previousMean);

    if (previousEnergy <= 1.0e-12)
        return result;

    float shape[256];
    const int count = std::min (points, 256);

    const auto similarity = [&] (double start)
    {
        sampleShape (x, (float) start, period, shape, count);
        double mean = 0.0;
        for (int i = 0; i < count; ++i)
            mean += shape[i];
        mean /= count;

        double product = 0.0, energy = 0.0;
        for (int i = 0; i < count; ++i)
        {
            const double a = shape[i] - mean;
            product += a * (previous[i] - previousMean);
            energy += a * a;
        }

        return energy > 1.0e-12 ? product / std::sqrt (energy * previousEnergy) : -1.0;
    };

    // Грубо - по сэмплам (для низких нот через один-два), потом точнее - параболой
    const int step = std::max (1, (int) (period / 256.0f));
    double best = latest, bestValue = -2.0;
    for (double start = latest; start >= earliest; start -= step)
    {
        const double value = similarity (start);
        if (value > bestValue)
        {
            bestValue = value;
            best = start;
        }
    }

    if (bestValue < 0.3)
        return result;

    if (best - step >= earliest && best + step <= latest)
    {
        const double left = similarity (best - step), right = similarity (best + step);
        const double denominator = left - 2.0 * bestValue + right;
        if (std::abs (denominator) > 1.0e-12)
            best += step * std::clamp (0.5 * (left - right) / denominator, -0.5, 0.5);
    }

    result.trigger = (float) best;
    result.period = period;
    result.found = true;
    return result;
}

// Бегущая волна: минимум и максимум по столбцам из bucket элементов, привязанным к абсолютному времени.
// endIndex - абсолютный номер элемента после последнего скопированного, count - сколько скопировано.
// Последний полный столбец кончается на кратном bucket номере, поэтому уже нарисованные столбцы
// от кадра к кадру не меняются. false - данных мало.
inline bool rollColumns (const float* minimums, const float* maximums, int count, long long endIndex,
                         int bucket, int buckets, float* lows, float* highs)
{
    const long long lastEnd = (endIndex / bucket) * bucket;
    const long long firstOffset = lastEnd - (long long) buckets * bucket - (endIndex - count);
    if (firstOffset < 0)
        return false;

    for (int b = 0; b < buckets; ++b)
    {
        const int from = (int) firstOffset + b * bucket;
        float low = minimums[from], high = maximums[from];

        for (int i = from + 1; i < from + bucket; ++i)
        {
            low = std::min (low, minimums[i]);
            high = std::max (high, maximums[i]);
        }

        lows[b] = low;
        highs[b] = high;
    }

    return true;
}

// Прежний способ: последний переход самой волны через ноль вверх (с гистерезисом) в зоне поиска
inline Result byZeroCrossing (const float* x, int search, float nominalPeriod, float peak)
{
    Result result;
    result.period = nominalPeriod;

    const float threshold = 0.08f * peak;
    float previous = -1.0f;
    bool armed = false;

    for (int i = 1; i < search; ++i)
    {
        const float a = x[i - 1], b = x[i];

        if (b < -threshold)
            armed = true;

        if (armed && a < 0.0f && b >= 0.0f)
        {
            if (result.found)
                previous = result.trigger;

            result.trigger = (float) (i - 1) + a / (a - b);
            result.found = true;
            armed = false;
        }
    }

    // Два перехода в зоне - период известен точно
    if (result.found && previous >= 0.0f)
    {
        const float measured = result.trigger - previous;
        if (measured > nominalPeriod * 0.8f && measured < nominalPeriod * 1.25f)
            result.period = measured;
    }

    return result;
}

} // namespace sonder::ScopeTrigger
