#include "VoiceManager.h"

namespace sonder
{

VoiceManager::VoiceManager()
{
    // Каждый "юнит" как отдельный экземпляр железного синта: свой фиксированный разброс деталей.
    // Seed постоянный, поэтому юнит #3 звучит одинаково от запуска к запуску.
    for (int u = 0; u < kNumUnits; ++u)
    {
        for (int v = 0; v < kMaxVoices; ++v)
        {
            FastRandom rng (0x5eed1234u + (uint32_t) (u * kMaxVoices + v + 1) * 0x9e3779b9u);

            for (int i = 0; i < 8; ++i)
                rng.nextUInt();

            auto& t = tolerances[(size_t) u][(size_t) v];
            t.oscCents[0]   = rng.nextBipolar();
            t.oscCents[1]   = rng.nextBipolar();
            t.cutoffOctaves = rng.nextBipolar();
            t.envelopeTime  = rng.nextBipolar();
            t.pulseWidth    = rng.nextBipolar();
            t.level         = rng.nextBipolar();
            t.pan           = rng.nextBipolar();
            t.vibratoRate   = rng.nextBipolar();

            // Третий и четвёртый осцилляторы появились позже: берём их в конце,
            // чтобы у старых юнитов остальной разброс остался прежним
            t.oscCents[2]   = rng.nextBipolar();
            t.oscCents[3]   = rng.nextBipolar();
        }
    }
}

void VoiceManager::prepare (double sampleRate, int maxBlockSize)
{
    for (auto& voice : voices)
        voice.prepare (sampleRate);

    modWheelBuffer.assign ((size_t) maxBlockSize, modWheelValue);
    aftertouchBuffer.assign ((size_t) maxBlockSize, aftertouchValue);
    pitchBendBuffer.assign ((size_t) maxBlockSize, 0.0f);

    controlSmoothingCoef = (float) (1.0 - std::exp (-1.0 / (0.005 * sampleRate)));

    numHeldNotes = 0;
    keyDown.fill (false);
    sustained.fill (false);
}

void VoiceManager::render (float* left, float* right, int numSamples, const juce::MidiBuffer& midi,
                           const SynthParams& params, ModulationBus bus)
{
    jassert (numSamples <= (int) modWheelBuffer.size());

    if (params.voiceMode != currentMode)
    {
        allNotesOff (false);
        currentMode = params.voiceMode;
    }

    int position = 0;

    for (const auto metadata : midi)
    {
        const int eventPosition = juce::jlimit (0, numSamples, metadata.samplePosition);

        if (eventPosition > position)
        {
            renderSegment (left, right, position, eventPosition, params, bus);
            position = eventPosition;
        }

        handleMessage (metadata.getMessage(), params, bus, eventPosition);
    }

    if (position < numSamples)
        renderSegment (left, right, position, numSamples, params, bus);
}

void VoiceManager::renderSegment (float* left, float* right, int start, int end,
                                  const SynthParams& params, ModulationBus& bus)
{
    // Контроллеры сглаживаются, иначе 7-битное колесо на срезе фильтра "ступенит"
    for (int i = start; i < end; ++i)
    {
        modWheelValue += (modWheelTarget - modWheelValue) * controlSmoothingCoef;
        aftertouchValue += (aftertouchTarget - aftertouchValue) * controlSmoothingCoef;
        pitchBendValue += (pitchBendTarget - pitchBendValue) * controlSmoothingCoef;

        modWheelBuffer[(size_t) i] = modWheelValue;
        aftertouchBuffer[(size_t) i] = aftertouchValue;
        pitchBendBuffer[(size_t) i] = pitchBendValue * params.bendRange;
    }

    bus.modWheel = modWheelBuffer.data();
    bus.aftertouch = aftertouchBuffer.data();
    bus.pitchBend = pitchBendBuffer.data();

    const auto& unitTolerances = tolerances[(size_t) juce::jlimit (0, kNumUnits - 1, params.unit)];

    // Источники для общих целей модуляции берутся у последнего взятого голоса
    for (size_t v = 0; v < voices.size(); ++v)
    {
        voices[v].setLeader ((int) v == lastStartedVoice);
        voices[v].render (left, right, start, end - start, params, bus, unitTolerances[v]);
    }
}

void VoiceManager::handleMessage (const juce::MidiMessage& message, const SynthParams& params,
                                  const ModulationBus& bus, int position)
{
    const int channel = juce::jlimit (1, 16, message.getChannel());
    const bool member = isMemberChannel (channel, params);

    // Голоса, которые играют на этом канале (MPE)
    const auto forChannel = [this, channel] (auto&& action)
    {
        for (auto& voice : voices)
            if (voice.isActive() && voice.getChannel() == channel)
                action (voice);
    };

    if (message.isNoteOn())
    {
        noteOn (message.getNoteNumber(), message.getFloatVelocity(), channel, params, bus, position);
    }
    else if (message.isNoteOff())
    {
        noteOff (message.getNoteNumber(), params);
    }
    else if (message.isPitchWheel())
    {
        const float bend = (float) (message.getPitchWheelValue() - 8192) / 8192.0f;

        if (member)
        {
            channelBend[(size_t) channel] = bend;
            forChannel ([&] (Voice& voice) { voice.setNoteBend (bend * params.mpeBendRange); });
        }
        else
        {
            pitchBendTarget = bend;
        }
    }
    else if (message.isChannelPressure())
    {
        const float pressure = (float) message.getChannelPressureValue() / 127.0f;

        if (member)
        {
            channelPressure[(size_t) channel] = pressure;
            forChannel ([&] (Voice& voice) { voice.setPressure (pressure); });
        }
        else
        {
            aftertouchTarget = pressure;
        }
    }
    else if (message.isAftertouch())
    {
        // Полифонический афтертач: давление одной клавиши
        const int note = message.getNoteNumber();
        const float pressure = (float) message.getAfterTouchValue() / 127.0f;

        for (auto& voice : voices)
            if (voice.isActive() && voice.getNote() == note)
                voice.setPressure (pressure);
    }
    else if (message.isAllSoundOff())
    {
        allNotesOff (true);
    }
    else if (message.isAllNotesOff())
    {
        allNotesOff (false);
    }
    else if (message.isController())
    {
        const int value = message.getControllerValue();

        switch (message.getControllerNumber())
        {
            case 1:
                modWheelTarget = (float) value / 127.0f;
                break;

            case 74:
            {
                // Слайд (MPE "третье измерение"): на канале ноты - только ей, иначе всем
                const float slide = (float) value / 127.0f;

                if (member)
                {
                    channelSlide[(size_t) channel] = slide;
                    forChannel ([&] (Voice& voice) { voice.setSlide (slide); });
                }
                else
                {
                    slideValue = slide;
                    for (auto& voice : voices)
                        voice.setSlide (slide);
                }
                break;
            }

            case 64:
                sustainPedal = value >= 64;
                if (! sustainPedal)
                    releaseSustainedNotes();
                break;

            default:
                break;
        }
    }
}

bool VoiceManager::shouldGlide (const SynthParams& params, bool overlapping) const noexcept
{
    if (params.glide <= 0.0f)
        return false;

    switch (params.glideMode)
    {
        case GlideMode::always:    return true;
        case GlideMode::legato:    return overlapping;
        case GlideMode::automatic: return currentMode == VoiceMode::legato ? overlapping : true;
    }

    return true;
}

void VoiceManager::startExpression (Voice& voice, int channel, const SynthParams& params)
{
    voice.setChannel (channel);

    if (isMemberChannel (channel, params))
        voice.setExpression (channelBend[(size_t) channel] * params.mpeBendRange, channelPressure[(size_t) channel],
                             channelSlide[(size_t) channel], true);
    else
        voice.setExpression (0.0f, 0.0f, slideValue, true);
}

void VoiceManager::noteOn (int note, float velocity, int channel, const SynthParams& params, const ModulationBus& bus, int position)
{
    // Связная игра: новая нота взята, пока ещё держится другая клавиша
    bool overlapping = false;
    for (int key = 0; key < 128 && ! overlapping; ++key)
        overlapping = keyDown[(size_t) key] && key != note;

    keyDown[(size_t) note] = true;
    sustained[(size_t) note] = false;

    const float pitch = pitchFor (note);
    lastNoteFrequency = 440.0f * std::exp2 ((pitch - 69.0f) / 12.0f);

    const bool glideNow = shouldGlide (params, overlapping);
    const float previousPitch = lastPitch;
    lastPitch = pitch;

    if (currentMode == VoiceMode::poly)
    {
        const int polyphony = params.unisonVoices <= 1 ? kMaxVoices : (params.unisonVoices == 2 ? 12 : 8);
        const float glideFrom = glideNow ? previousPitch : -1.0f;

        // Та же нота уже звучит (например, затухает): перезапускаем её голос, а не занимаем новый
        int index = -1;
        for (int v = 0; v < polyphony; ++v)
            if (voices[(size_t) v].isActive() && voices[(size_t) v].getNote() == note)
                index = v;

        if (index < 0)
            index = chooseVoice (polyphony);

        auto& voice = voices[(size_t) index];

        // Голос занят другой нотой: сначала быстрый фейд старой, потом старт новой.
        // Повтор той же ноты фейда не требует: высота не меняется, огибающая продолжит с текущего уровня.
        if (voice.isActive() && (voice.isStealing() || voice.getNote() != note))
            voice.stealTo (note, pitch, velocity, glideFrom);
        else
            voice.start (note, pitch, velocity, glideFrom, true, params, bus, position);

        startExpression (voice, channel, params);

        voiceOrder[(size_t) index] = ++orderCounter;
        lastStartedVoice = index;
        nextVoice = (index + 1) % polyphony;
        return;
    }

    auto& voice = voices[0];
    const bool hadHeldNotes = numHeldNotes > 0;
    pushHeldNote (note);

    if (currentMode == VoiceMode::legato && hadHeldNotes && voice.isActive())
    {
        voice.glideTo (note, pitch, glideNow);
        startExpression (voice, channel, params);
        return;
    }

    float glideFrom = -1.0f;
    if (glideNow)
        glideFrom = voice.isActive() ? voice.getCurrentPitch() : previousPitch;

    voice.start (note, pitch, velocity, glideFrom, true, params, bus, position);
    startExpression (voice, channel, params);
    lastStartedVoice = 0;
}

int VoiceManager::chooseVoice (int polyphony) const
{
    // Ротация: следующий свободный голос после последнего занятого, как на Juno/Prophet
    for (int i = 0; i < polyphony; ++i)
    {
        const int index = (nextVoice + i) % polyphony;
        if (! voices[(size_t) index].isActive())
            return index;
    }

    // Свободных нет: крадём самый старый затухающий, иначе просто самый старый
    int oldest = -1;
    for (int pass = 0; pass < 2 && oldest < 0; ++pass)
    {
        for (int v = 0; v < polyphony; ++v)
        {
            if (pass == 0 && ! voices[(size_t) v].isReleasing())
                continue;

            if (oldest < 0 || voiceOrder[(size_t) v] < voiceOrder[(size_t) oldest])
                oldest = v;
        }
    }

    return oldest;
}

void VoiceManager::noteOff (int note, const SynthParams& params)
{
    keyDown[(size_t) note] = false;

    if (currentMode == VoiceMode::poly)
    {
        if (sustainPedal)
        {
            sustained[(size_t) note] = true;
            return;
        }

        for (auto& voice : voices)
            if (voice.isActive() && ! voice.isReleasing() && voice.getNote() == note)
                voice.release();

        return;
    }

    removeHeldNote (note);
    auto& voice = voices[0];

    if (numHeldNotes > 0)
    {
        // Возвращаемся к предыдущей зажатой ноте без перезапуска огибающих
        const int previous = heldNotes[(size_t) numHeldNotes - 1];
        if (voice.getNote() != previous)
        {
            lastPitch = pitchFor (previous);
            voice.glideTo (previous, lastPitch, shouldGlide (params, true));
        }
    }
    else if (sustainPedal)
    {
        sustained[(size_t) note] = true;
    }
    else if (voice.isActive())
    {
        voice.release();
    }
}

void VoiceManager::releaseSustainedNotes()
{
    for (int note = 0; note < 128; ++note)
    {
        if (! sustained[(size_t) note])
            continue;

        sustained[(size_t) note] = false;

        if (keyDown[(size_t) note])
            continue;

        if (currentMode == VoiceMode::poly)
        {
            for (auto& voice : voices)
                if (voice.isActive() && ! voice.isReleasing() && voice.getNote() == note)
                    voice.release();
        }
        else if (numHeldNotes == 0 && voices[0].isActive())
        {
            voices[0].release();
        }
    }
}

void VoiceManager::allNotesOff (bool immediately)
{
    for (auto& voice : voices)
    {
        if (immediately)
            voice.kill();
        else if (voice.isActive())
            voice.release();
    }

    numHeldNotes = 0;
    keyDown.fill (false);
    sustained.fill (false);
    channelBend.fill (0.0f);
    channelPressure.fill (0.0f);
}

const Voice* VoiceManager::getDisplayVoice() const noexcept
{
    if (juce::isPositiveAndBelow (lastStartedVoice, kMaxVoices) && voices[(size_t) lastStartedVoice].isActive())
        return &voices[(size_t) lastStartedVoice];

    return nullptr;
}

uint32_t VoiceManager::getActiveVoiceMask() const noexcept
{
    uint32_t mask = 0;
    for (size_t v = 0; v < voices.size(); ++v)
        if (voices[v].isActive())
            mask |= 1u << v;

    return mask;
}

void VoiceManager::pushHeldNote (int note) noexcept
{
    removeHeldNote (note);
    if (numHeldNotes < (int) heldNotes.size())
        heldNotes[(size_t) numHeldNotes++] = note;
}

void VoiceManager::removeHeldNote (int note) noexcept
{
    for (int i = 0; i < numHeldNotes; ++i)
    {
        if (heldNotes[(size_t) i] == note)
        {
            for (int j = i; j < numHeldNotes - 1; ++j)
                heldNotes[(size_t) j] = heldNotes[(size_t) j + 1];

            --numHeldNotes;
            return;
        }
    }
}

} // namespace sonder
