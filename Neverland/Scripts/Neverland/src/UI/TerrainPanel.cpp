#include "TerrainPanel.hpp"
#include "../Model/BlockPalette.hpp"

TerrainPanel::TerrainPanel(const ArchUIContext &context)
    : m_context(context) {
    addSubview(ArchUI::sectionLabel("Sculpting materials"));
    auto row = ArchUI::cardsRow();
    for (const BlockPalette::BlockEntry &entry : BlockPalette::TERRAIN_MATERIALS) {
        auto card = std::make_shared<BlockCardButton>(
            entry.name, context.previewFor(entry.type), nullptr
        );
        // Выбор материала лепки выходит из режима постройки (форма сбрасывается на куб).
        card->setOnClick([context, type = entry.type](PandaUI::Button &) {
            if (!context.blocks) { return; }
            context.blocks->setSelectedElement(ArchObjectType::Block);
            context.blocks->setSelectedBlock(type);
        });
        if (context.blockCards != nullptr) { context.blockCards->emplace_back(entry.type, card); }
        row->addSubview(card);
    }
    addSubview(row);

    const auto makeRow = [this](const char *title) {
        auto rowPanel = std::make_shared<PandaUI::Panel>();
        rowPanel->setBackgroundColor(PandaUI::Color(0x00000000));
        rowPanel->layout().setFlexDirection(PandaUI::FlexDirection::Row);
        rowPanel->layout().setAlignItems(PandaUI::Align::Center);
        rowPanel->layout().setGap(6.f);
        rowPanel->layout().setWidth(PandaUI::Length::percent(100.f));
        auto titleLabel = std::make_shared<PandaUI::Label>(title);
        titleLabel->setFont(PandaUI::Font(12.f));
        titleLabel->setTextColor(PandaUI::Color(0xD5DCE8FF));
        titleLabel->layout().setWidth(PandaUI::Length::points(88.f));
        rowPanel->addSubview(titleLabel);
        addSubview(rowPanel);
        return rowPanel;
    };

    addSubview(ArchUI::sectionLabel("Brush"));
    auto modeRow = makeRow("Mode (B)");
    for (const GameBrushMode mode :
         {GameBrushMode::Sphere, GameBrushMode::Raise, GameBrushMode::Flatten}) {
        auto button = std::make_shared<SettingsPresetButton>(GameBrush::modeName(mode));
        button->setOnClick([this, mode](PandaUI::Button &) {
            if (m_context.blocks) { m_context.blocks->setBrushMode(mode); }
            refresh();
        });
        m_modeButtons.emplace_back(mode, button);
        modeRow->addSubview(button);
    }

    auto sizeRow = makeRow("Size ( [ ] )");
    static const char *const SIZE_NAMES[] = {"1", "2", "3", "4"};
    for (int size = 0; size < BlocksCreation::brushSizeCount(); ++size) {
        const char *name = size < 4 ? SIZE_NAMES[size] : "+";
        auto button = std::make_shared<SettingsPresetButton>(name);
        button->setOnClick([this, size](PandaUI::Button &) {
            if (m_context.blocks) { m_context.blocks->setBrushSize(size); }
            refresh();
        });
        m_sizeButtons.emplace_back(size, button);
        sizeRow->addSubview(button);
    }

    addSubview(ArchUI::noteLabel(
        "Terrain is sculpted, not built: LMB adds, RMB removes. Building mode ignores terrain."
    ));
    refresh();
}

void TerrainPanel::refresh() {
    if (!m_context.blocks) { return; }
    const GameBrushMode mode = m_context.blocks->getBrushMode();
    for (auto &[value, button] : m_modeButtons) {
        button->setSelected(value == mode);
    }
    const int size = m_context.blocks->getBrushSize();
    for (auto &[value, button] : m_sizeButtons) {
        button->setSelected(value == size);
    }
}
