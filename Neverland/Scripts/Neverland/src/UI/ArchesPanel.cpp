#include "ArchesPanel.hpp"

ArchesPanel::ArchesPanel(const ArchUIContext &) {
    addSubview(ArchUI::noteLabel("Arched openings for wall runs — in development."));
    ArchUI::addPlannedSection(
        *this, {
                   "Form: semicircular / segmental / pointed / elliptical",
                   "Keystone",
                   "Archivolt",
               }
    );
}
