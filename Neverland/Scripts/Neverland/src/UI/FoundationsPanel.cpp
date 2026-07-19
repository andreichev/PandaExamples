#include "FoundationsPanel.hpp"

FoundationsPanel::FoundationsPanel(const ArchUIContext &context) {
    auto row = ArchUI::cardsRow();
    row->addSubview(ArchUI::makeElementCard(context, ArchObjectType::Block, "Basic", "1 x 1"));
    row->addSubview(ArchUI::makeElementCard(context, ArchObjectType::Beam, "Beam", "3 x 1"));
    addSubview(row);
    addSubview(ArchUI::noteLabel(
        "Solid shapes for footings, plinths and beams. Built from the current material; drag to "
        "lay a line."
    ));

    ArchUI::addPlannedSection(
        *this, {
                   "Plinth profile",
                   "Blind area",
                   "Entrance steps",
               }
    );
}
