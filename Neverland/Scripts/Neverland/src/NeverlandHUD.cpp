#include "NeverlandHUD.hpp"

#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/Logger.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <string>

namespace {

PandaUI::Color transparent() {
    return PandaUI::Color(0x00000000);
}

std::shared_ptr<PandaUI::Label> makeLabel(const std::string &text, float fontSize, PandaUI::Color color) {
    auto label = std::make_shared<PandaUI::Label>(text);
    label->setFontSize(fontSize);
    label->setTextColor(color);
    return label;
}

} // namespace

void NeverlandHUD::start() {
    m_blocksCreation = EntityAPI::getScript<BlocksCreation>(getEntity());
    buildUI();
    updateSelection();
}

void NeverlandHUD::update(float) {
    updateSelection();
}

void NeverlandHUD::shutdown() {
    if (m_window.isValid()) { m_window.setRootView(nullptr); }
    m_slots.fill(nullptr);
    m_root.reset();
    m_blocksCreation.reset();
    m_displayedSelection = VoxelType::NOTHING;
}

void NeverlandHUD::buildUI() {
    m_window = PandaUI::Window::main();
    if (!m_window.isValid()) {
        LOG_ERROR("NeverlandHUD: main PandaUI window is unavailable");
        return;
    }

    m_root = std::make_shared<PandaUI::Panel>();
    m_root->setBackgroundColor(transparent());
    m_root->style().setFlexDirection(PandaUI::FlexDirection::Column);
    m_root->style().setWidth(PandaUI::Length::percent(100.f));
    m_root->style().setHeight(PandaUI::Length::percent(100.f));

    auto safeArea = std::make_shared<PandaUI::SafeAreaView>();
    safeArea->setBackgroundColor(transparent());
    safeArea->addSubview(makeHotbar());

    m_root->addSubview(makeCrosshair());
    m_root->addSubview(safeArea);
    m_window.setRootView(m_root);
}

std::shared_ptr<PandaUI::Panel> NeverlandHUD::makeCrosshair() {
    auto crosshair = std::make_shared<PandaUI::Panel>();
    crosshair->setBackgroundColor(transparent());
    crosshair->styleSetAbsolute();
    crosshair->style().setPosition(PandaUI::Edge::Left, PandaUI::Length::percent(50.f));
    crosshair->style().setPosition(PandaUI::Edge::Top, PandaUI::Length::percent(50.f));
    crosshair->style().setWidth(PandaUI::Length::points(34.f));
    crosshair->style().setHeight(PandaUI::Length::points(34.f));
    crosshair->setTransform(glm::translate(glm::mat4(1.f), {-17.f, -17.f, 0.f}));

    auto horizontal = std::make_shared<PandaUI::Panel>();
    horizontal->setBackgroundColor(PandaUI::Color(0xFFFFFFFF));
    horizontal->styleSetAbsolute();
    horizontal->style().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(2.f));
    horizontal->style().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(16.f));
    horizontal->style().setWidth(PandaUI::Length::points(30.f));
    horizontal->style().setHeight(PandaUI::Length::points(2.f));

    auto vertical = std::make_shared<PandaUI::Panel>();
    vertical->setBackgroundColor(PandaUI::Color(0xFFFFFFFF));
    vertical->styleSetAbsolute();
    vertical->style().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(16.f));
    vertical->style().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(2.f));
    vertical->style().setWidth(PandaUI::Length::points(2.f));
    vertical->style().setHeight(PandaUI::Length::points(30.f));

    crosshair->addSubview(horizontal);
    crosshair->addSubview(vertical);
    return crosshair;
}

std::shared_ptr<PandaUI::Panel> NeverlandHUD::makeHotbar() {
    auto hotbar = std::make_shared<PandaUI::Panel>();
    hotbar->setBackgroundColor(PandaUI::Color(0x111319CC));
    hotbar->setClipsToBounds(true);
    hotbar->styleSetAbsolute();
    hotbar->style().setPosition(PandaUI::Edge::Left, PandaUI::Length::percent(50.f));
    hotbar->style().setPosition(PandaUI::Edge::Bottom, PandaUI::Length::points(24.f));
    hotbar->style().setWidth(PandaUI::Length::points(472.f));
    hotbar->style().setHeight(PandaUI::Length::points(68.f));
    hotbar->style().setPadding(PandaUI::Edge::Horizontal, 10.f);
    hotbar->style().setPadding(PandaUI::Edge::Vertical, 8.f);
    hotbar->style().setGap(8.f);
    hotbar->style().setFlexDirection(PandaUI::FlexDirection::Row);
    hotbar->setTransform(glm::translate(glm::mat4(1.f), {-236.f, 0.f, 0.f}));

    for (size_t i = 0; i < BLOCKS.size(); ++i) {
        auto slot = makeSlot(BLOCKS[i], static_cast<int>(i));
        m_slots[i] = slot;
        hotbar->addSubview(slot);
    }
    return hotbar;
}

std::shared_ptr<PandaUI::Panel> NeverlandHUD::makeSlot(const BlockSlot &slot, int index) {
    auto view = std::make_shared<PandaUI::Panel>();
    view->setClipsToBounds(true);
    view->style().setWidth(PandaUI::Length::points(48.f));
    view->style().setHeight(PandaUI::Length::points(52.f));
    view->style().setPadding(4.f);
    view->style().setGap(3.f);
    view->style().setFlexDirection(PandaUI::FlexDirection::Column);
    view->style().setAlignItems(PandaUI::Align::Center);
    view->style().setJustifyContent(PandaUI::Justify::Center);
    view->setBackgroundColor(PandaUI::Color(0x2D3440DD));
    view->setOpacity(0.82f);

    auto color = std::make_shared<PandaUI::Panel>();
    color->setBackgroundColor(PandaUI::Color(slot.color));
    color->style().setWidth(PandaUI::Length::points(32.f));
    color->style().setHeight(PandaUI::Length::points(24.f));
    view->addSubview(color);

    auto label = makeLabel(std::to_string(index + 1), 12.f, PandaUI::Color(0xE9EDF4FF));
    view->addSubview(label);

    return view;
}

void NeverlandHUD::updateSelection() {
    if (!m_blocksCreation) { return; }
    VoxelType selected = m_blocksCreation->getSelectedBlock();
    if (selected == m_displayedSelection) { return; }

    for (size_t i = 0; i < BLOCKS.size(); ++i) {
        applySlotStyle(i, BLOCKS[i].type == selected);
    }
    m_displayedSelection = selected;
}

void NeverlandHUD::applySlotStyle(size_t index, bool selected) {
    if (!m_slots[index]) { return; }
    m_slots[index]->setBackgroundColor(selected ? PandaUI::Color(0xF0F4FFFF) : PandaUI::Color(0x2D3440DD));
    m_slots[index]->setOpacity(selected ? 1.f : 0.82f);
}
