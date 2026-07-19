#pragma once

#include "ArchUICommon.hpp"

// Раздел «Материалы»: чистый выбор материала — форма (element) не меняется.
class MaterialsPanel final : public ArchPanel {
public:
    explicit MaterialsPanel(const ArchUIContext &context);
    void refresh() override;
    bool hasOwnScrolling() const override {
        return true;
    }

private:
    std::shared_ptr<MaterialGridView> m_grid;
};
