#include "ModelsPanel.hpp"
#include "../Model/BlockModel.hpp"

ModelsPanel::ModelsPanel(const ArchUIContext &context, uint8_t category, const char *note)
    : m_context(context) {
    size_t modelCount = 0;
    const BlockModelData *models = BlockModels::all(modelCount);
    constexpr size_t CARDS_PER_ROW = 4;
    std::shared_ptr<PandaUI::Panel> row;
    size_t cardIndex = 0;
    const auto nextRow = [&]() {
        if (cardIndex % CARDS_PER_ROW == 0) {
            row = ArchUI::cardsRow();
            addSubview(row);
        }
        cardIndex++;
    };
    for (size_t i = 0; i < modelCount; i++) {
        const BlockModelData &model = models[i];
        if (model.category != category) { continue; }
        nextRow();
        auto card = std::make_shared<BlockCardButton>(model.name, PandaUI::TextureHandle{}, "◆");
        // Смена только параметра модели не видна dirty-check'у HUD — панель
        // обновляет подсветку сама сразу по клику.
        card->setOnClick([this, context, id = model.id](PandaUI::Button &) {
            if (!context.blocks) { return; }
            context.blocks->setElementParam(ArchObjectType::ModelBlock, 0, id);
            context.blocks->setSelectedElement(ArchObjectType::ModelBlock);
            refresh();
        });
        m_modelCards.emplace_back(model.id, card);
        row->addSubview(card);
    }
    if (category == 4) { // Decor: фонарь-источник света живёт здесь
        nextRow();
        row->addSubview(ArchUI::makeElementCard(context, ArchObjectType::Lamp, "Lantern", "light"));
    }
    if (note != nullptr) { addSubview(ArchUI::noteLabel(note)); }
}

void ModelsPanel::refresh() {
    if (!m_context.blocks) { return; }
    const bool modelSelected =
        m_context.blocks->getSelectedElement() == ArchObjectType::ModelBlock;
    const uint8_t current = m_context.blocks->getElementParam(ArchObjectType::ModelBlock, 0);
    for (auto &[id, card] : m_modelCards) {
        card->setCardSelected(modelSelected && id == current);
    }
}
