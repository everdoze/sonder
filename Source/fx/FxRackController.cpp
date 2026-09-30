#include "FxRackController.h"
#include "Parameters.h"
#include "synth/SynthParams.h"

namespace sonder
{

FxRackController::FxRackController (FxRack& r, juce::AudioProcessorValueTreeState& s, juce::AudioProcessor& p)
    : rack (r), state (s), processor (p)
{
}

std::vector<int> FxRackController::getChain() const
{
    std::vector<int> chain;
    for (int slot : rack.getOrder())
        if (rack.getType (slot) != FxType::none)
            chain.push_back (slot);

    return chain;
}

juce::RangedAudioParameter* FxRackController::getParameter (int slot, int param) const
{
    return state.getParameter (ParamIDs::fxParam (slot, param));
}

juce::RangedAudioParameter* FxRackController::getOnParameter (int slot) const
{
    return state.getParameter (ParamIDs::fxOn (slot));
}

float FxRackController::getRealValue (int slot, int param) const
{
    const auto& info = getFxTypeInfo (rack.getType (slot));
    if (! juce::isPositiveAndBelow (param, (int) info.params.size()))
        return 0.0f;

    return info.params[(size_t) param].toReal (state.getRawParameterValue (ParamIDs::fxParam (slot, param))->load());
}

void FxRackController::setRealValue (int slot, int param, float real)
{
    if (! juce::isPositiveAndBelow (slot, kNumFxSlots))
        return;

    const auto& info = getFxTypeInfo (rack.getType (slot));
    auto* parameter = getParameter (slot, param);

    if (parameter == nullptr || ! juce::isPositiveAndBelow (param, (int) info.params.size()))
        return;

    parameter->setValueNotifyingHost (info.params[(size_t) param].toNormalised (real));
}

void FxRackController::setDefaults (int slot, FxType type)
{
    const auto& info = getFxTypeInfo (type);

    for (int i = 0; i < kNumFxParams; ++i)
    {
        if (auto* parameter = getParameter (slot, i))
        {
            const float value = i < (int) info.params.size()
                                  ? info.params[(size_t) i].toNormalised (info.params[(size_t) i].defaultValue)
                                  : 0.0f;
            parameter->setValueNotifyingHost (value);
        }
    }

    if (auto* on = getOnParameter (slot))
        on->setValueNotifyingHost (1.0f);
}

void FxRackController::clearModulation (int slot)
{
    // Связи матрицы с ручками слота теряют смысл, когда в слоте меняется эффект
    for (int modSlot = 0; modSlot < kNumModSlots; ++modSlot)
    {
        int targetSlot = -1, param = -1;
        const auto dest = static_cast<ModDest> ((int) state.getRawParameterValue (ParamIDs::modDest (modSlot))->load());

        if (! fxSlotAndParam (dest, targetSlot, param) || targetSlot != slot)
            continue;

        for (const auto& id : { ParamIDs::modSource (modSlot), ParamIDs::modDest (modSlot),
                                ParamIDs::modAmount (modSlot), ParamIDs::modPolarity (modSlot) })
            if (auto* parameter = state.getParameter (id))
                parameter->setValueNotifyingHost (parameter->getDefaultValue());
    }
}

void FxRackController::notifyHost()
{
    // Имена параметров слотов зависят от вида эффекта: просим хост перечитать их
    processor.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withParameterInfoChanged (true));
}

int FxRackController::addEffect (FxType type)
{
    if (type == FxType::none)
        return -1;

    for (int slot = 0; slot < kNumFxSlots; ++slot)
    {
        if (rack.getType (slot) != FxType::none)
            continue;

        rack.setType (slot, type);
        setDefaults (slot, type);
        moveEffect (slot, kNumFxSlots); // в конец цепочки
        notifyHost();
        return slot;
    }

    return -1;
}

void FxRackController::removeEffect (int slot)
{
    clearModulation (slot);
    rack.setType (slot, FxType::none);
    notifyHost();
}

void FxRackController::moveEffect (int slot, int chainPosition)
{
    const auto chain = getChain();

    // Слот, перед которым нужно встать (-1 - в конец цепочки)
    const int before = juce::isPositiveAndBelow (chainPosition, (int) chain.size()) ? chain[(size_t) chainPosition] : -1;
    if (before == slot)
        return;

    const auto current = rack.getOrder();
    std::vector<int> order (current.begin(), current.end());
    order.erase (std::remove (order.begin(), order.end(), slot), order.end());

    auto insertAt = order.end();

    if (before >= 0)
    {
        insertAt = std::find (order.begin(), order.end(), before);
    }
    else
    {
        // После последнего занятого слота
        for (auto it = order.begin(); it != order.end(); ++it)
            if (rack.getType (*it) != FxType::none && *it != slot)
                insertAt = it + 1;

        if (chain.empty() || (chain.size() == 1 && chain.front() == slot))
            insertAt = order.begin();
    }

    order.insert (insertAt, slot);

    FxRack::Order result {};
    std::copy (order.begin(), order.end(), result.begin());
    rack.setOrder (result);
}

void FxRackController::clear (bool removeModulation)
{
    for (int slot = 0; slot < kNumFxSlots; ++slot)
    {
        if (removeModulation && rack.getType (slot) != FxType::none)
            clearModulation (slot);

        rack.setType (slot, FxType::none);
    }

    rack.setOrder (FxRack::defaultOrder());
    notifyHost();
}

std::unique_ptr<juce::XmlElement> FxRackController::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("FxRack");

    juce::StringArray order;
    for (int slot : rack.getOrder())
        order.add (juce::String (slot));

    xml->setAttribute ("order", order.joinIntoString (","));

    for (int slot = 0; slot < kNumFxSlots; ++slot)
    {
        const auto type = rack.getType (slot);
        if (type == FxType::none)
            continue;

        auto* child = xml->createNewChildElement ("Slot");
        child->setAttribute ("index", slot);
        child->setAttribute ("type", getFxTypeInfo (type).id);
    }

    return xml;
}

void FxRackController::fromXml (const juce::XmlElement& xml)
{
    for (int slot = 0; slot < kNumFxSlots; ++slot)
        rack.setType (slot, FxType::none);

    for (auto* child : xml.getChildWithTagNameIterator ("Slot"))
    {
        const int slot = child->getIntAttribute ("index", -1);
        if (juce::isPositiveAndBelow (slot, kNumFxSlots))
            rack.setType (slot, fxTypeFromId (child->getStringAttribute ("type")));
    }

    const auto parts = juce::StringArray::fromTokens (xml.getStringAttribute ("order"), ",", {});
    FxRack::Order order = FxRack::defaultOrder();

    if (parts.size() == kNumFxSlots)
        for (int i = 0; i < kNumFxSlots; ++i)
            order[(size_t) i] = parts[i].getIntValue();

    if (! rack.setOrder (order))
        rack.setOrder (FxRack::defaultOrder());

    notifyHost();
}

} // namespace sonder
