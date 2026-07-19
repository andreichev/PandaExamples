#include "ArchUICommon.hpp"
#include "../NeverlandHUDLayout.hpp"

namespace {

// Контент ячейки сетки материалов: превью + подпись. Пассивный (клики и подсветку
// выделения обрабатывает сама коллекция).
class MaterialCellContent final : public PandaUI::Panel {
public:
    MaterialCellContent() {
        setBackgroundColor(PandaUI::Color(0x00000000));
        layout().setFlexDirection(PandaUI::FlexDirection::Column);
        layout().setAlignItems(PandaUI::Align::Center);
        layout().setJustifyContent(PandaUI::Justify::Center);
        layout().setGap(4.f);

        m_preview = std::make_shared<PandaUI::ImageView>();
        m_preview->setContentMode(PandaUI::ImageContentMode::Fit);
        m_preview->setUserInteractionEnabled(false);
        m_preview->layout().setSize(
            {NeverlandHUDLayout::MenuCardPreview, NeverlandHUDLayout::MenuCardPreview}
        );
        addSubview(m_preview);

        m_label = std::make_shared<PandaUI::Label>("");
        m_label->setFont(PandaUI::Font(11.f));
        m_label->setTextColor(PandaUI::Color(0xD5DCE8FF));
        addSubview(m_label);
    }

    void bind(const char *name, PandaUI::TextureHandle texture) {
        m_label->setText(name);
        m_preview->setTexture(texture);
        m_preview->setHidden(!texture);
    }

private:
    std::shared_ptr<PandaUI::ImageView> m_preview;
    std::shared_ptr<PandaUI::Label> m_label;
};

} // namespace

BlockCardButton::BlockCardButton(const char *name, PandaUI::TextureHandle preview, const char *fallbackText)
    : PandaUI::Button("")
    , m_selected(false) {
    setFocusable(false);
    setClipsToBounds(true);
    surface().setCornerRadius(10.f);
    surface().setBorderWidth(1.5f);
    layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::MenuCardWidth));
    layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::MenuCardHeight));
    layout().setPadding(6.f);
    layout().setGap(4.f);
    layout().setFlexDirection(PandaUI::FlexDirection::Column);
    layout().setAlignItems(PandaUI::Align::Center);
    layout().setJustifyContent(PandaUI::Justify::Center);
    getTitleLabel()->setHidden(true);

    auto previewArea = std::make_shared<PandaUI::Panel>();
    previewArea->setBackgroundColor(PandaUI::Color(0x00000000));
    previewArea->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::MenuCardPreview));
    previewArea->layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::MenuCardPreview));
    previewArea->layout().setAlignItems(PandaUI::Align::Center);
    previewArea->layout().setJustifyContent(PandaUI::Justify::Center);
    if (preview) {
        auto image = std::make_shared<PandaUI::ImageView>(preview);
        image->setContentMode(PandaUI::ImageContentMode::Fit);
        image->setUserInteractionEnabled(false);
        image->layout().setWidth(PandaUI::Length::percent(100.f));
        image->layout().setHeight(PandaUI::Length::percent(100.f));
        previewArea->addSubview(image);
    } else if (fallbackText) {
        auto text = std::make_shared<PandaUI::Label>(fallbackText);
        text->setFont(PandaUI::Font(16.f));
        text->setTextColor(PandaUI::Color(0xF4F7FFFF));
        previewArea->addSubview(text);
    }
    addSubview(previewArea);

    auto label = std::make_shared<PandaUI::Label>(name);
    label->setFont(PandaUI::Font(11.f));
    label->setTextColor(PandaUI::Color(0xD5DCE8FF));
    addSubview(label);
    updateAppearance();
}

void BlockCardButton::setCardSelected(bool selected) {
    if (m_selected == selected) { return; }
    m_selected = selected;
    updateAppearance();
}

void BlockCardButton::controlStateChanged(PandaUI::ControlState, PandaUI::ControlState) {
    updateAppearance();
}

void BlockCardButton::themeChanged() {
    updateAppearance();
}

void BlockCardButton::updateAppearance() {
    if (hasControlState(PandaUI::ControlState::Highlighted)) {
        setBackgroundColor(PandaUI::Color(0x414A59FF));
        surface().setBorderColor(PandaUI::Color(0xFFFFFFFF));
    } else if (hasControlState(PandaUI::ControlState::Hovered)) {
        setBackgroundColor(PandaUI::Color(0x39424FEE));
        surface().setBorderColor(PandaUI::Color(0xFFFFFFAA));
    } else {
        setBackgroundColor(PandaUI::Color(0x2A313CDD));
        surface().setBorderColor(
            m_selected ? PandaUI::Color(0xFFFFFFFF) : PandaUI::Color(0x5C667666)
        );
    }
}

MenuButton::MenuButton(std::string text)
    : PandaUI::Button(std::move(text)) {
    setFocusable(false);
    setFont(PandaUI::Font(20.f));
    getTitleLabel()->setTextColor(PandaUI::Color(0xF4F7FFFF));
    surface().setCornerRadius(14.f);
    surface().setBorderWidth(1.5f);
    layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::MenuButtonWidth));
    layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::MenuButtonHeight));
    layout().setPadding(0.f);
    updateAppearance();
}

void MenuButton::controlStateChanged(PandaUI::ControlState, PandaUI::ControlState) {
    updateAppearance();
}

void MenuButton::themeChanged() {
    updateAppearance();
}

void MenuButton::updateAppearance() {
    if (hasControlState(PandaUI::ControlState::Highlighted)) {
        setBackgroundColor(PandaUI::Color(0xF4F7FFFF));
        getTitleLabel()->setTextColor(PandaUI::Color(0x111722FF));
        surface().setBorderColor(PandaUI::Color(0xFFFFFFFF));
    } else if (hasControlState(PandaUI::ControlState::Hovered)) {
        setBackgroundColor(PandaUI::Color(0x2A3342E6));
        getTitleLabel()->setTextColor(PandaUI::Color(0xFFFFFFFF));
        surface().setBorderColor(PandaUI::Color(0xFFFFFFAA));
    } else {
        setBackgroundColor(PandaUI::Color(0x151B26CC));
        getTitleLabel()->setTextColor(PandaUI::Color(0xF4F7FFFF));
        surface().setBorderColor(PandaUI::Color(0xFFFFFF66));
    }
    surface().setShadowColor(PandaUI::Color(0x00000038));
}

SettingsPresetButton::SettingsPresetButton(std::string text)
    : PandaUI::Button(std::move(text)) {
    setFocusable(false);
    setFont(PandaUI::Font(12.f));
    surface().setCornerRadius(8.f);
    surface().setBorderWidth(1.f);
    layout().setHeight(PandaUI::Length::points(26.f));
    layout().setPadding(PandaUI::Edge::Horizontal, 10.f);
    layout().setPadding(PandaUI::Edge::Vertical, 0.f);
    updateAppearance();
}

void SettingsPresetButton::setSelected(bool selected) {
    if (m_selected == selected) { return; }
    m_selected = selected;
    updateAppearance();
}

void SettingsPresetButton::controlStateChanged(PandaUI::ControlState, PandaUI::ControlState) {
    updateAppearance();
}

void SettingsPresetButton::themeChanged() {
    updateAppearance();
}

void SettingsPresetButton::updateAppearance() {
    if (hasControlState(PandaUI::ControlState::Highlighted) || m_selected) {
        setBackgroundColor(PandaUI::Color(0xE8EEF8E6));
        getTitleLabel()->setTextColor(PandaUI::Color(0x111722FF));
        surface().setBorderColor(PandaUI::Color(0xFFFFFFFF));
    } else if (hasControlState(PandaUI::ControlState::Hovered)) {
        setBackgroundColor(PandaUI::Color(0x414A59EE));
        getTitleLabel()->setTextColor(PandaUI::Color(0xF4F7FFFF));
        surface().setBorderColor(PandaUI::Color(0xFFFFFFAA));
    } else {
        setBackgroundColor(PandaUI::Color(0x2D3440DD));
        getTitleLabel()->setTextColor(PandaUI::Color(0xF4F7FFFF));
        surface().setBorderColor(PandaUI::Color(0x5C6676AA));
    }
}

ArchPanel::ArchPanel() {
    setBackgroundColor(PandaUI::Color(0x00000000));
    layout().setFlexDirection(PandaUI::FlexDirection::Column);
    layout().setGap(10.f);
    layout().setWidth(PandaUI::Length::percent(100.f));
}

PresetRow::PresetRow(
    const ArchUIContext &context, ArchObjectType element, const char *title, int paramIndex,
    std::vector<Option> options
)
    : m_context(context)
    , m_element(element)
    , m_paramIndex(paramIndex) {
    setBackgroundColor(PandaUI::Color(0x00000000));
    layout().setFlexDirection(PandaUI::FlexDirection::Row);
    layout().setAlignItems(PandaUI::Align::Center);
    layout().setGap(6.f);
    layout().setWidth(PandaUI::Length::percent(100.f));

    auto titleLabel = std::make_shared<PandaUI::Label>(title);
    titleLabel->setFont(PandaUI::Font(12.f));
    titleLabel->setTextColor(PandaUI::Color(0xD5DCE8FF));
    titleLabel->layout().setWidth(PandaUI::Length::points(88.f));
    addSubview(titleLabel);

    for (const Option &option : options) {
        auto button = std::make_shared<SettingsPresetButton>(option.name);
        button->setOnClick([this, value = option.value](PandaUI::Button &) {
            if (!m_context.blocks) { return; }
            m_context.blocks->setElementParam(m_element, m_paramIndex, value);
            m_context.blocks->setSelectedElement(m_element); // настройка = выбор формы
            syncFromState();
        });
        m_buttons.emplace_back(option.value, button);
        addSubview(button);
    }
    syncFromState();
}

void PresetRow::syncFromState() {
    if (!m_context.blocks) { return; }
    const uint8_t current = m_context.blocks->getElementParam(m_element, m_paramIndex);
    for (auto &[value, button] : m_buttons) {
        button->setSelected(value == current);
    }
}

MaterialGridView::MaterialGridView(
    const ArchUIContext &context, std::vector<Entry> entries, ArchObjectType elementOnSelect,
    bool selectElement
)
    : PandaUI::CollectionView(std::make_shared<PandaUI::GridCollectionLayout>(
          PandaUI::Size(NeverlandHUDLayout::MenuCardWidth, NeverlandHUDLayout::MenuCardHeight),
          8.f, 4.f
      ))
    , m_context(context)
    , m_entries(std::move(entries))
    , m_elementOnSelect(elementOnSelect)
    , m_selectElement(selectElement) {
    setSelectionMode(PandaUI::CollectionSelectionMode::Single);

    PandaUI::CollectionViewDataSource source;
    source.numberOfItems = [this] { return static_cast<int>(m_entries.size()); };
    source.cellForReuse = [] { return std::make_shared<MaterialCellContent>(); };
    source.configureCell = [this](PandaUI::View &cell, int index) {
        const Entry &entry = m_entries[static_cast<size_t>(index)];
        static_cast<MaterialCellContent &>(cell).bind(
            entry.name,
            m_context.previewFor ? m_context.previewFor(entry.type) : PandaUI::TextureHandle{}
        );
    };
    setDataSource(source);

    setOnSelectionChanged([this](PandaUI::CollectionView &) {
        if (!m_context.blocks || selectedIndices().empty()) { return; }
        const Entry &entry = m_entries[static_cast<size_t>(selectedIndices().front())];
        m_context.blocks->setSelectedBlock(entry.type);
        if (m_selectElement) { m_context.blocks->setSelectedElement(m_elementOnSelect); }
    });
    reloadData();
}

void MaterialGridView::syncFromState() {
    if (!m_context.blocks) { return; }
    const VoxelType selected = m_context.blocks->getSelectedBlock();
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].type == selected) {
            const int index = static_cast<int>(i);
            if (!isSelected(index)) { setSelectedIndices({index}); }
            return;
        }
    }
    clearSelection();
}

namespace ArchUI {

void setViewVisible(PandaUI::View &view, bool visible) {
    view.setHidden(!visible);
    view.layout().setDisplay(visible ? PandaUI::Display::Flex : PandaUI::Display::None);
}

std::shared_ptr<PandaUI::Label> sectionLabel(const char *text) {
    auto label = std::make_shared<PandaUI::Label>(text);
    label->setFont(PandaUI::Font(13.f));
    label->setTextColor(PandaUI::Color(0x9AA6BAFF));
    return label;
}

std::shared_ptr<PandaUI::Label> noteLabel(const char *text) {
    auto label = std::make_shared<PandaUI::Label>(text);
    label->setFont(PandaUI::Font(11.f));
    label->setTextColor(PandaUI::Color(0x6E7A8EFF));
    return label;
}

std::shared_ptr<PandaUI::Panel> cardsRow() {
    auto row = std::make_shared<PandaUI::Panel>();
    row->setBackgroundColor(PandaUI::Color(0x00000000));
    row->layout().setFlexDirection(PandaUI::FlexDirection::Row);
    row->layout().setGap(8.f);
    return row;
}

std::shared_ptr<PandaUI::Panel> divider() {
    auto line = std::make_shared<PandaUI::Panel>();
    line->setBackgroundColor(PandaUI::Color(0xFFFFFF1E));
    line->layout().setWidth(PandaUI::Length::percent(100.f));
    line->layout().setHeight(PandaUI::Length::points(1.f));
    line->layout().setMargin(PandaUI::Edge::Vertical, 4.f);
    return line;
}

std::shared_ptr<PandaUI::Label> plannedLabel(const char *text) {
    auto label = std::make_shared<PandaUI::Label>(text);
    label->setFont(PandaUI::Font(12.f));
    label->setTextColor(PandaUI::Color(0x55607255));
    return label;
}

void addPlannedSection(PandaUI::Panel &panel, std::initializer_list<const char *> items) {
    panel.addSubview(divider());
    panel.addSubview(sectionLabel("Planned parameters"));
    for (const char *item : items) {
        panel.addSubview(plannedLabel(item));
    }
}

std::shared_ptr<BlockCardButton> makeMaterialCard(
    const ArchUIContext &context, VoxelType type, const char *name,
    ArchObjectType elementOnSelect, bool selectElement
) {
    auto card = std::make_shared<BlockCardButton>(name, context.previewFor(type), nullptr);
    card->setOnClick([context, type, elementOnSelect, selectElement](PandaUI::Button &) {
        if (!context.blocks) { return; }
        context.blocks->setSelectedBlock(type);
        if (selectElement) { context.blocks->setSelectedElement(elementOnSelect); }
    });
    if (context.blockCards != nullptr) { context.blockCards->emplace_back(type, card); }
    return card;
}

std::shared_ptr<BlockCardButton> makeElementCard(
    const ArchUIContext &context, ArchObjectType element, const char *name, const char *hint
) {
    auto card = std::make_shared<BlockCardButton>(name, PandaUI::TextureHandle{}, hint);
    card->setOnClick([context, element](PandaUI::Button &) {
        if (context.blocks) { context.blocks->setSelectedElement(element); }
    });
    if (context.elementCards != nullptr) { context.elementCards->emplace_back(element, card); }
    return card;
}

} // namespace ArchUI
