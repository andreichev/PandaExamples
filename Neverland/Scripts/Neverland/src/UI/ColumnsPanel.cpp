#include "ColumnsPanel.hpp"

ColumnsPanel::ColumnsPanel(const ArchUIContext &) {
    addSubview(ArchUI::noteLabel("Free-standing classical columns — in development."));
    ArchUI::addPlannedSection(
        *this, {
                   "Order: Tuscan / Doric / Ionic / Corinthian / Composite",
                   "Entasis",
                   "Flutes",
                   "Base and capital",
                   "Pedestal",
               }
    );
}
