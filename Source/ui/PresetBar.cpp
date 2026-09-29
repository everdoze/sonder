#include "PresetBar.h"
#include "SonderLookAndFeel.h"

namespace sonder::ui
{

PresetBar::PresetBar (PresetManager& manager)
    : presets (manager)
{
    for (auto* button : { &previousButton, &nextButton, &nameButton, &saveButton })
        addAndMakeVisible (button);

    previousButton.onClick = [this] { presets.loadPrevious(); };
    nextButton.onClick = [this] { presets.loadNext(); };
    nameButton.onClick = [this] { showPresetMenu(); };
    saveButton.onClick = [this] { showSaveDialog(); };

    previousButton.setTooltip ("Previous preset");
    nextButton.setTooltip ("Next preset");
    saveButton.setTooltip ("Save current sound as a user preset");

    presets.addChangeListener (this);
    updateName();
}

PresetBar::~PresetBar()
{
    presets.removeChangeListener (this);
}

void PresetBar::updateName()
{
    const int index = presets.getCurrentIndex();
    const auto& list = presets.getPresets();
    const auto category = juce::isPositiveAndBelow (index, (int) list.size()) ? list[(size_t) index].category : juce::String();

    nameButton.setButtonText (category.isNotEmpty() && category != "Init"
                                  ? category.toUpperCase() + "  /  " + presets.getCurrentName()
                                  : presets.getCurrentName());
}

void PresetBar::resized()
{
    auto area = getLocalBounds();
    saveButton.setBounds (area.removeFromRight (64));
    area.removeFromRight (8);
    previousButton.setBounds (area.removeFromLeft (30));
    nextButton.setBounds (area.removeFromRight (30));
    nameButton.setBounds (area.reduced (4, 0));
}

void PresetBar::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto& list = presets.getPresets();

    // Пресеты по категориям, в порядке появления
    juce::StringArray categories;
    for (const auto& preset : list)
        categories.addIfNotAlreadyThere (preset.category);

    for (const auto& category : categories)
    {
        menu.addSectionHeader (category.toUpperCase());

        for (int i = 0; i < (int) list.size(); ++i)
            if (list[(size_t) i].category == category)
                menu.addItem (i + 1, list[(size_t) i].name, true, i == presets.getCurrentIndex());
    }

    const int current = presets.getCurrentIndex();
    const bool canDelete = juce::isPositiveAndBelow (current, (int) list.size()) && ! list[(size_t) current].isFactory;

    constexpr int saveId = 10000, deleteId = 10001, folderId = 10002;
    menu.addSeparator();
    menu.addItem (saveId, "Save as...");
    menu.addItem (deleteId, "Delete current preset", canDelete);
    menu.addItem (folderId, "Open presets folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton).withMinimumWidth (nameButton.getWidth()),
                        [this, current] (int result)
                        {
                            if (result == saveId)
                                showSaveDialog();
                            else if (result == deleteId)
                                presets.deleteUserPreset (current);
                            else if (result == folderId)
                            {
                                const auto folder = PresetManager::getUserPresetDirectory();
                                folder.createDirectory();
                                folder.startAsProcess();
                            }
                            else if (result > 0)
                                presets.load (result - 1);
                        });
}

void PresetBar::showSaveDialog()
{
    auto* window = new juce::AlertWindow ("Save preset", "Name for the new user preset:", juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("name", presets.getCurrentName());
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    // Колбэк вызывается до удаления окна (deleteWhenDismissed = true)
    window->enterModalState (true, juce::ModalCallbackFunction::create ([this, window] (int result)
    {
        if (result == 1)
            presets.saveUserPreset (window->getTextEditorContents ("name"));
    }), true);
}

} // namespace sonder::ui
