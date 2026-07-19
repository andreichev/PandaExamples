#include "BalconiesPanel.hpp"

BalconiesPanel::BalconiesPanel(const ArchUIContext &) {
    addSubview(ArchUI::noteLabel("Balconies on brackets — in development."));
    ArchUI::addPlannedSection(
        *this, {
                   "Slab",
                   "Railing: balustrade / wrought iron / stone",
                   "Brackets",
                   "French balcony",
               }
    );
}
