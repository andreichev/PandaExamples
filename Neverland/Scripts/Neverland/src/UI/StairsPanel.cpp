#include "StairsPanel.hpp"

StairsPanel::StairsPanel(const ArchUIContext &) {
    addSubview(ArchUI::noteLabel("Outdoor stairs and porches — in development."));
    ArchUI::addPlannedSection(
        *this, {
                   "Type: straight / L-shaped / spiral",
                   "Steps and risers",
                   "Railing",
                   "Porch",
               }
    );
}
