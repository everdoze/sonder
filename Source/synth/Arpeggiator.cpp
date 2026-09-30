#include "Arpeggiator.h"

#include <algorithm>
#include <limits>

namespace sonder
{

void Arpeggiator::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    reset();
}

void Arpeggiator::reset()
{
    numHeld = 0;
    numPhysicallyDown = 0;
    physicallyDown.fill (false);
    patternSize = 0;
    clockRunning = false;
    nextStep = 0;
    position = 0;
    lastIndex = -1;
    sounding = -1;
    offCountdown = 0.0;
    lastStepValid = false;
    publish();
}

void Arpeggiator::process (juce::MidiBuffer& midi, int numSamples, const Settings& settings,
                           double bpm, std::optional<double> hostPpq)
{
    if (! settings.on)
    {
        // Выключили: отпускаем звучащую ноту, дальше MIDI идёт как есть
        if (wasOn)
        {
            if (sounding >= 0)
                midi.addEvent (juce::MidiMessage::noteOff (1, sounding), 0);

            reset();
            wasOn = false;
        }

        return;
    }

    if (! wasOn)
    {
        reset();
        wasOn = true;
    }

    beatsPerSample = juce::jmax (20.0, bpm) / 60.0 / sampleRate;
    stepSamples = settings.stepBeats / beatsPerSample;

    // Hold выключили: забываем ноты, которые уже не держат
    if (! settings.hold)
    {
        int kept = 0;
        for (int i = 0; i < numHeld; ++i)
            if (physicallyDown[(size_t) held[(size_t) i].note])
                held[(size_t) kept++] = held[(size_t) i];

        if (kept != numHeld)
        {
            numHeld = kept;
            rebuildPattern (settings);
        }
    }
    else
    {
        rebuildPattern (settings); // режим или число октав могли смениться
    }

    // С транспортом хоста шаги стоят на его сетке
    synced = hostPpq.has_value();
    if (synced)
    {
        clockBeats = *hostPpq;
        clockRunning = true;
        nextStep = (long long) std::floor (clockBeats / settings.stepBeats) - 1;
        while (stepBeat (nextStep, settings) < clockBeats - 1.0e-9)
            ++nextStep;
    }

    juce::MidiBuffer out;
    int from = 0;

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const int at = juce::jlimit (0, numSamples, metadata.samplePosition);

        runSteps (out, from, at, settings);
        from = at;

        if (message.isNoteOn())
        {
            const bool wasEmpty = numHeld == 0 && sounding < 0;
            noteDown (message.getNoteNumber(), message.getVelocity(), settings);

            // Свой такт начинается с первой нотой: она звучит сразу
            if (! synced && wasEmpty)
            {
                clockRunning = true;
                nextStep = 0;
                clockBeats = -(double) at * beatsPerSample;
                lastIndex = -1;
                position = 0;
                lastStepValid = false;
            }
            else
            {
                catchUpStep (out, at, settings);
            }
        }
        else if (message.isNoteOff())
        {
            noteUp (message.getNoteNumber(), settings);

            if (numHeld == 0)
            {
                if (sounding >= 0)
                    stopNote (out, at);

                if (! synced)
                    clockRunning = false;

                position = 0;
            }
            else
            {
                catchUpStep (out, at, settings);
            }
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            if (sounding >= 0)
                stopNote (out, at);

            numHeld = 0;
            numPhysicallyDown = 0;
            physicallyDown.fill (false);
            rebuildPattern (settings);
            out.addEvent (message, at);
        }
        else
        {
            out.addEvent (message, at);
        }
    }

    runSteps (out, from, numSamples, settings);

    clockBeats += numSamples * beatsPerSample;
    lastStepAt -= numSamples;
    if (sounding >= 0)
        offCountdown -= numSamples;

    midi.swapWith (out);
}

double Arpeggiator::stepBeat (long long step, const Settings& settings) const noexcept
{
    // Каждый второй шаг опаздывает на Swing: 0.5 шага на максимуме ручки задаёт сильный шаффл
    const bool odd = (step & 1) != 0;
    return (double) step * settings.stepBeats + (odd ? settings.swing * 0.5 * settings.stepBeats : 0.0);
}

void Arpeggiator::runSteps (juce::MidiBuffer& out, int from, int to, const Settings& settings)
{
    constexpr long long never = std::numeric_limits<long long>::max();

    for (;;)
    {
        // Позиции в целых сэмплах. Шаг, который попадает на сэмпл to, играется уже после нот этого сэмпла:
        // иначе на смене аккорда шаг успевал сыграть старую ноту, а новая ждала следующего шага
        const long long offAt = sounding >= 0 ? (long long) std::floor (offCountdown) : never;
        const long long stepAt = clockRunning
                                   ? (long long) std::ceil ((stepBeat (nextStep, settings) - clockBeats) / beatsPerSample - 1.0e-6)
                                   : never;

        if (offAt >= to && stepAt >= to)
            return;

        if (offAt <= stepAt)
        {
            stopNote (out, (int) juce::jlimit ((long long) from, (long long) to - 1, offAt));
            continue;
        }

        playStep (out, (int) juce::jlimit ((long long) from, (long long) to - 1, stepAt), settings);
        ++nextStep;
    }
}

void Arpeggiator::catchUpStep (juce::MidiBuffer& out, int at, const Settings& settings)
{
    // Ноты сменились сразу после шага (клип или живая игра чуть позже сетки): шаг играется заново
    // уже с новыми нотами, а не пропадает до следующего
    if (! clockRunning || ! lastStepValid || patternSize == 0 || at < lastStepAt)
        return;

    const double tolerance = juce::jmin (0.25 * stepSamples, 0.03 * sampleRate, 0.9 * settings.gate * stepSamples);
    if ((double) at - lastStepAt > tolerance)
        return;

    bool stale = sounding < 0;

    if (! stale)
    {
        if (settings.mode == Mode::random)
            stale = std::find (pattern.begin(), pattern.begin() + patternSize, sounding) == pattern.begin() + patternSize;
        else
            stale = pattern[(size_t) (lastStepPosition % patternSize)] != sounding;
    }

    if (! stale)
        return;

    if (sounding >= 0)
        stopNote (out, at);

    position = lastStepPosition;
    playStep (out, at, settings);
}

void Arpeggiator::playStep (juce::MidiBuffer& out, int at, const Settings& settings)
{
    if (sounding >= 0)
        stopNote (out, at);

    lastStepAt = at;
    lastStepPosition = position;
    lastStepValid = true;

    if (patternSize == 0)
        return;

    int index = position % patternSize;

    if (settings.mode == Mode::random)
    {
        index = random.nextInt (patternSize);
        if (patternSize > 1 && index == lastIndex)
            index = (index + 1) % patternSize;
    }

    lastIndex = index;
    ++position;

    sounding = pattern[(size_t) index];
    out.addEvent (juce::MidiMessage::noteOn (1, sounding, patternVelocity[(size_t) index]), at);

    // Нота звучит долю шага, но не короче 3 мс
    offCountdown = at + juce::jmax (0.003 * sampleRate, (double) settings.gate * stepSamples);
    displayStep.store (index);
}

void Arpeggiator::stopNote (juce::MidiBuffer& out, int at)
{
    out.addEvent (juce::MidiMessage::noteOff (1, sounding), at);
    sounding = -1;
    displayStep.store (-1);
}

void Arpeggiator::noteDown (int note, juce::uint8 velocity, const Settings& settings)
{
    // С Hold новый аккорд, взятый после того, как все клавиши отпустили, заменяет старый
    if (settings.hold && numPhysicallyDown == 0)
        numHeld = 0;

    if (! physicallyDown[(size_t) note])
    {
        physicallyDown[(size_t) note] = true;
        ++numPhysicallyDown;
    }

    for (int i = 0; i < numHeld; ++i)
        if (held[(size_t) i].note == note)
            return;

    if (numHeld < kMaxNotes)
        held[(size_t) numHeld++] = { note, velocity };

    rebuildPattern (settings);
}

void Arpeggiator::noteUp (int note, const Settings& settings)
{
    if (physicallyDown[(size_t) note])
    {
        physicallyDown[(size_t) note] = false;
        --numPhysicallyDown;
    }

    if (settings.hold)
        return;

    for (int i = 0; i < numHeld; ++i)
    {
        if (held[(size_t) i].note == note)
        {
            for (int j = i; j < numHeld - 1; ++j)
                held[(size_t) j] = held[(size_t) j + 1];

            --numHeld;
            break;
        }
    }

    rebuildPattern (settings);
}

void Arpeggiator::rebuildPattern (const Settings& settings)
{
    std::array<HeldNote, kMaxNotes> base {};
    std::copy_n (held.begin(), numHeld, base.begin());

    if (settings.mode != Mode::asPlayed)
        std::sort (base.begin(), base.begin() + numHeld, [] (const HeldNote& a, const HeldNote& b) { return a.note < b.note; });

    // Ноты по октавам вверх
    std::array<HeldNote, kMaxPattern> expanded {};
    int size = 0;
    for (int octave = 0; octave < juce::jlimit (1, 4, settings.octaves); ++octave)
        for (int i = 0; i < numHeld; ++i)
            if (const int note = base[(size_t) i].note + 12 * octave; note <= 127 && size < kMaxPattern)
                expanded[(size_t) size++] = { note, base[(size_t) i].velocity };

    patternSize = 0;
    const auto add = [this] (const HeldNote& note)
    {
        if (patternSize < kMaxPattern)
        {
            pattern[(size_t) patternSize] = note.note;
            patternVelocity[(size_t) patternSize] = note.velocity;
            ++patternSize;
        }
    };

    switch (settings.mode)
    {
        case Mode::down:
            for (int i = size - 1; i >= 0; --i)
                add (expanded[(size_t) i]);
            break;

        case Mode::upDown:
            // Вверх и обратно, крайние ноты не повторяются
            for (int i = 0; i < size; ++i)
                add (expanded[(size_t) i]);
            for (int i = size - 2; i >= 1; --i)
                add (expanded[(size_t) i]);
            break;

        case Mode::up:
        case Mode::random:
        case Mode::asPlayed:
            for (int i = 0; i < size; ++i)
                add (expanded[(size_t) i]);
            break;
    }

    publish();
}

void Arpeggiator::publish()
{
    for (int i = 0; i < patternSize; ++i)
        displayPattern[(size_t) i].store (pattern[(size_t) i]);

    displaySize.store (patternSize);
}

} // namespace sonder
