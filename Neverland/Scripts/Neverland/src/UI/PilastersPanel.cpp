#include "PilastersPanel.hpp"

PilastersPanel::PilastersPanel(const ArchUIContext &) {
    addSubview(ArchUI::noteLabel("Flat wall-mounted columns — in development."));
    ArchUI::addPlannedSection(
        *this, {
                   "Order (matches columns)",
                   "Flutes",
                   "Width and projection",
               }
    );
}
