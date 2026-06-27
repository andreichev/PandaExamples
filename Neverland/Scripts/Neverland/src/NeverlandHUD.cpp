#include "NeverlandHUD.hpp"

#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/Logger.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <string>

namespace {

PandaUI::Color transparent() {
    return PandaUI::Color(0x00000000);
}

bool shouldShowCrosshair() {
#if defined(NEVERLAND_MOBILE)
    return false;
#else
    return true;
#endif
}

std::shared_ptr<PandaUI::Label>
makeLabel(const std::string &text, float fontSize, PandaUI::Color color) {
    auto label = std::make_shared<PandaUI::Label>(text);
    label->setFont(PandaUI::Font(fontSize));
    label->setTextColor(color);
    return label;
}

class HotbarSlotButton final : public PandaUI::Button {
public:
    HotbarSlotButton()
        : PandaUI::Button("")
        , m_selected(false) {
        updateSlotAppearance();
    }

    void setSlotSelected(bool selected) {
        if (m_selected == selected) { return; }
        m_selected = selected;
        setOpacity(m_selected ? 1.f : 0.82f);
        updateSlotAppearance();
    }

private:
    void controlStateChanged(PandaUI::ControlState, PandaUI::ControlState) override {
        updateSlotAppearance();
    }

    void themeChanged() override {
        updateSlotAppearance();
    }

    void updateSlotAppearance() {
        if (hasControlState(PandaUI::ControlState::Highlighted)) {
            setBackgroundColor(PandaUI::Color(0xF0F4FFFF));
        } else if (hasControlState(PandaUI::ControlState::Hovered)) {
            setBackgroundColor(m_selected ? PandaUI::Color(0xFFFFFFFF) : PandaUI::Color(0x414A59EE));
        } else {
            setBackgroundColor(m_selected ? PandaUI::Color(0xF0F4FFFF) : PandaUI::Color(0x2D3440DD));
        }
        surface().setBorderColor(m_selected ? PandaUI::Color(0xFFFFFFFF) : PandaUI::Color(0x5C6676AA));
        surface().setShadowColor(m_selected ? PandaUI::Color(0x00000038) : PandaUI::Color(0x00000024));
    }

    bool m_selected;
};

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
    m_root->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    m_root->layout().setWidth(PandaUI::Length::percent(100.f));
    m_root->layout().setHeight(PandaUI::Length::percent(100.f));

    auto safeArea = std::make_shared<PandaUI::SafeAreaView>();
    safeArea->setBackgroundColor(transparent());
    safeArea->addSubview(makeHotbar());

    if (shouldShowCrosshair()) { m_root->addSubview(makeCrosshair()); }
    m_root->addSubview(safeArea);
    m_window.setRootView(m_root);
}

std::shared_ptr<PandaUI::Panel> NeverlandHUD::makeCrosshair() {
    auto crosshair = std::make_shared<PandaUI::Panel>();
    crosshair->setBackgroundColor(transparent());
    crosshair->layoutSetAbsolute();
    crosshair->layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::percent(50.f));
    crosshair->layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::percent(50.f));
    crosshair->layout().setWidth(PandaUI::Length::points(34.f));
    crosshair->layout().setHeight(PandaUI::Length::points(34.f));
    crosshair->setTransform(glm::translate(glm::mat4(1.f), {-17.f, -17.f, 0.f}));

    auto horizontal = std::make_shared<PandaUI::Panel>();
    horizontal->setBackgroundColor(PandaUI::Color(0xFFFFFFFF));
    horizontal->layoutSetAbsolute();
    horizontal->layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(2.f));
    horizontal->layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(16.f));
    horizontal->layout().setWidth(PandaUI::Length::points(30.f));
    horizontal->layout().setHeight(PandaUI::Length::points(2.f));

    auto vertical = std::make_shared<PandaUI::Panel>();
    vertical->setBackgroundColor(PandaUI::Color(0xFFFFFFFF));
    vertical->layoutSetAbsolute();
    vertical->layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(16.f));
    vertical->layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(2.f));
    vertical->layout().setWidth(PandaUI::Length::points(2.f));
    vertical->layout().setHeight(PandaUI::Length::points(30.f));

    crosshair->addSubview(horizontal);
    crosshair->addSubview(vertical);
    return crosshair;
}

std::shared_ptr<PandaUI::Panel> NeverlandHUD::makeHotbar() {
    auto hotbar = std::make_shared<PandaUI::Panel>();
    hotbar->setBackgroundColor(PandaUI::Color(0x111319CC));
    hotbar->setClipsToBounds(true);
    hotbar->layoutSetAbsolute();
    hotbar->layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::percent(50.f));
    hotbar->layout().setPosition(PandaUI::Edge::Bottom, PandaUI::Length::points(24.f));
    hotbar->layout().setWidth(PandaUI::Length::points(472.f));
    hotbar->layout().setHeight(PandaUI::Length::points(68.f));
    hotbar->layout().setPadding(PandaUI::Edge::Horizontal, 10.f);
    hotbar->layout().setPadding(PandaUI::Edge::Vertical, 8.f);
    hotbar->layout().setGap(8.f);
    hotbar->layout().setFlexDirection(PandaUI::FlexDirection::Row);
    hotbar->setTransform(glm::translate(glm::mat4(1.f), {-236.f, 0.f, 0.f}));

    for (size_t i = 0; i < BLOCKS.size(); ++i) {
        auto slot = makeSlot(BLOCKS[i], static_cast<int>(i));
        m_slots[i] = slot;
        hotbar->addSubview(slot);
    }
    return hotbar;
}

std::shared_ptr<PandaUI::Button> NeverlandHUD::makeSlot(const BlockSlot &slot, int index) {
    auto view = std::make_shared<HotbarSlotButton>();
    view->setFocusable(false);
    view->setClipsToBounds(true);
    view->layout().setWidth(PandaUI::Length::points(48.f));
    view->layout().setHeight(PandaUI::Length::points(52.f));
    view->layout().setPadding(4.f);
    view->layout().setGap(3.f);
    view->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    view->layout().setAlignItems(PandaUI::Align::Center);
    view->layout().setJustifyContent(PandaUI::Justify::Center);
    view->setOpacity(0.82f);
    view->getTitleLabel()->setHidden(true);
    view->setOnClick([this, type = slot.type](PandaUI::Button &) {
        if (!m_blocksCreation) { return; }
        m_blocksCreation->setSelectedBlock(type);
        updateSelection();
    });

    auto color = std::make_shared<PandaUI::Panel>();
    color->setBackgroundColor(PandaUI::Color(slot.color));
    color->layout().setWidth(PandaUI::Length::points(32.f));
    color->layout().setHeight(PandaUI::Length::points(24.f));
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
    auto slot = std::dynamic_pointer_cast<HotbarSlotButton>(m_slots[index]);
    if (!slot) { return; }
    slot->setSlotSelected(selected);
}
