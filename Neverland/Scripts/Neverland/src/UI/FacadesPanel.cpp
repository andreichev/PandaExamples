#include "FacadesPanel.hpp"

FacadesPanel::FacadesPanel(const ArchUIContext &) {
    addSubview(ArchUI::noteLabel(
        "Pick a ready-made facade, then take it apart into individual elements. Styles will also "
        "work as library filters."
    ));

    static const char *const STYLES[] = {
        "Classicism", "Baroque",  "Empire", "Neoclassicism",      "Art Nouveau",
        "Art Deco",   "Italian",  "French", "St. Petersburg XIX", "Custom",
    };
    constexpr size_t CHIPS_PER_ROW = 5;
    std::shared_ptr<PandaUI::Panel> row;
    for (size_t i = 0; i < std::size(STYLES); ++i) {
        if (i % CHIPS_PER_ROW == 0) {
            row = ArchUI::cardsRow();
            addSubview(row);
        }
        auto chip = std::make_shared<SettingsPresetButton>(STYLES[i]);
        chip->setOpacity(0.45f); // заготовка: пресеты фасадов ещё не собраны
        row->addSubview(chip);
    }

    addSubview(ArchUI::divider());
    addSubview(ArchUI::plannedLabel("Facade presets are in development"));
}
