#include "DecorPanel.hpp"

DecorPanel::DecorPanel(const ArchUIContext &context) {
    addSubview(ArchUI::sectionLabel("Lighting"));
    auto row = ArchUI::cardsRow();
    row->addSubview(ArchUI::makeElementCard(context, ArchObjectType::Lamp, "Lantern", "light"));
    addSubview(row);
    addSubview(ArchUI::noteLabel("Glowing lantern: lights nearby blocks and terrain at night."));

    ArchUI::addPlannedSection(
        *this, {
                   "Bas-reliefs",
                   "Coats of arms",
                   "Mascarons",
                   "Rosettes",
                   "Medallions",
                   "Vases",
                   "Statues",
                   "Clock",
                   "Flags",
                   "Plaques",
               }
    );
}
