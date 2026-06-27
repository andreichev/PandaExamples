#include "NeverlandHUD.hpp"
#include "NeverlandTouchControls.hpp"

#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/Logger.hpp>

#include <functional>
#include <string>
#include <utility>

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

bool shouldShowTouchControls() {
#if defined(NEVERLAND_MOBILE)
    return true;
#else
    return false;
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
    bool pointerDown(PandaUI::PointerEvent &event) override {
        if (event.type == PandaUI::PointerType::Touch) { NeverlandTouchControls::ignoreTouch(event.pointerId); }
        return PandaUI::Button::pointerDown(event);
    }

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

class PassThroughPanel final : public PandaUI::Panel {
public:
    PandaUI::View *hitTest(PandaUI::Point windowPosition) override {
        for (auto it = getSubviews().rbegin(); it != getSubviews().rend(); ++it) {
            PandaUI::View *hitView = (*it)->hitTest(windowPosition);
            if (hitView) { return hitView; }
        }
        return nullptr;
    }
};

class PassThroughSafeAreaView final : public PandaUI::SafeAreaView {
public:
    PandaUI::View *hitTest(PandaUI::Point windowPosition) override {
        for (auto it = getSubviews().rbegin(); it != getSubviews().rend(); ++it) {
            PandaUI::View *hitView = (*it)->hitTest(windowPosition);
            if (hitView) { return hitView; }
        }
        return nullptr;
    }
};

class TouchControlButton final : public PandaUI::Button {
public:
    using PressCallback = std::function<void(bool)>;

    explicit TouchControlButton(std::string text)
        : PandaUI::Button(std::move(text))
        , m_pressed(false) {
        setFocusable(false);
        setFont(PandaUI::Font(16.f));
        getTitleLabel()->setTextColor(PandaUI::Color(0xF4F7FFFF));
        surface().setCornerRadius(18.f);
        surface().setBorderWidth(1.5f);
        updateAppearance();
    }

    void setOnPressedChanged(PressCallback callback) {
        m_callback = std::move(callback);
    }

private:
    bool pointerDown(PandaUI::PointerEvent &event) override {
        if (event.type == PandaUI::PointerType::Touch) { NeverlandTouchControls::ignoreTouch(event.pointerId); }
        return PandaUI::Button::pointerDown(event);
    }

    void controlStateChanged(PandaUI::ControlState, PandaUI::ControlState) override {
        const bool pressed = hasControlState(PandaUI::ControlState::Highlighted);
        if (m_pressed != pressed) {
            m_pressed = pressed;
            if (m_callback) { m_callback(m_pressed); }
        }
        updateAppearance();
    }

    void themeChanged() override {
        updateAppearance();
    }

    void updateAppearance() {
        if (hasControlState(PandaUI::ControlState::Highlighted)) {
            setBackgroundColor(PandaUI::Color(0xF4F7FFFF));
            getTitleLabel()->setTextColor(PandaUI::Color(0x111722FF));
            surface().setBorderColor(PandaUI::Color(0xFFFFFFFF));
            surface().setShadowColor(PandaUI::Color(0x00000044));
        } else {
            setBackgroundColor(PandaUI::Color(0x151B26CC));
            getTitleLabel()->setTextColor(PandaUI::Color(0xF4F7FFFF));
            surface().setBorderColor(PandaUI::Color(0xFFFFFF66));
            surface().setShadowColor(PandaUI::Color(0x00000030));
        }
    }

    PressCallback m_callback;
    bool m_pressed;
};

std::shared_ptr<TouchControlButton>
makeTouchControlButton(const std::string &text, NeverlandTouchControls::Button button) {
    auto view = std::make_shared<TouchControlButton>(text);
    view->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::TouchButtonSize));
    view->layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::TouchButtonSize));
    view->layout().setPadding(0.f);
    view->setOnPressedChanged([button](bool pressed) {
        NeverlandTouchControls::setPressed(button, pressed);
    });
    return view;
}

} // namespace

void NeverlandHUD::start() {
    NeverlandTouchControls::reset();
    m_blocksCreation = EntityAPI::getScript<BlocksCreation>(getEntity());
    buildUI();
    updateTouchControlSafeArea();
    updateSelection();
}

void NeverlandHUD::update(float) {
    updateTouchControlSafeArea();
    updateSelection();
}

void NeverlandHUD::shutdown() {
    NeverlandTouchControls::reset();
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
    safeArea->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    safeArea->layout().setJustifyContent(PandaUI::Justify::End);
    safeArea->layout().setAlignItems(PandaUI::Align::Center);
    safeArea->addSubview(makeHotbar());

    if (shouldShowCrosshair()) { m_root->addSubview(makeCrosshair()); }
    m_root->addSubview(safeArea);
    if (shouldShowTouchControls()) { m_root->addSubview(makeTouchControls()); }
    m_window.setRootView(m_root);
}

void NeverlandHUD::updateTouchControlSafeArea() {
    if (!m_window.isValid()) { return; }
    const PandaUI::EdgeInsets insets = m_window.getSafeAreaInsets();
    NeverlandTouchControls::setSafeAreaInsets(insets.top, insets.left, insets.right, insets.bottom);
}

std::shared_ptr<PandaUI::Panel> NeverlandHUD::makeCrosshair() {
    auto overlay = std::make_shared<PandaUI::Panel>();
    overlay->setBackgroundColor(transparent());
    overlay->setUserInteractionEnabled(false);
    overlay->layoutSetAbsolute();
    overlay->layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(0.f));
    overlay->layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(0.f));
    overlay->layout().setWidth(PandaUI::Length::percent(100.f));
    overlay->layout().setHeight(PandaUI::Length::percent(100.f));
    overlay->layout().setJustifyContent(PandaUI::Justify::Center);
    overlay->layout().setAlignItems(PandaUI::Align::Center);

    auto crosshair = std::make_shared<PandaUI::Panel>();
    crosshair->setBackgroundColor(transparent());
    crosshair->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::CrosshairSize));
    crosshair->layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::CrosshairSize));

    auto horizontal = std::make_shared<PandaUI::Panel>();
    horizontal->setBackgroundColor(PandaUI::Color(0xFFFFFFFF));
    horizontal->layoutSetAbsolute();
    horizontal->layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(NeverlandHUDLayout::CrosshairLineInset));
    horizontal->layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(NeverlandHUDLayout::CrosshairLineCenter));
    horizontal->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::CrosshairLineLength));
    horizontal->layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::CrosshairLineThickness));

    auto vertical = std::make_shared<PandaUI::Panel>();
    vertical->setBackgroundColor(PandaUI::Color(0xFFFFFFFF));
    vertical->layoutSetAbsolute();
    vertical->layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(NeverlandHUDLayout::CrosshairLineCenter));
    vertical->layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(NeverlandHUDLayout::CrosshairLineInset));
    vertical->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::CrosshairLineThickness));
    vertical->layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::CrosshairLineLength));

    crosshair->addSubview(horizontal);
    crosshair->addSubview(vertical);
    overlay->addSubview(crosshair);
    return overlay;
}

std::shared_ptr<PandaUI::Panel> NeverlandHUD::makeTouchControls() {
    auto overlay = std::make_shared<PassThroughSafeAreaView>();
    overlay->setBackgroundColor(transparent());
    overlay->layoutSetAbsolute();
    overlay->layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(0.f));
    overlay->layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(0.f));
    overlay->layout().setWidth(PandaUI::Length::percent(100.f));
    overlay->layout().setHeight(PandaUI::Length::percent(100.f));

    auto content = std::make_shared<PassThroughPanel>();
    content->setBackgroundColor(transparent());
    content->layout().setWidth(PandaUI::Length::percent(100.f));
    content->layout().setFlexGrow(1.f);
    content->addSubview(makeMovePad());
    content->addSubview(makeActionControls());
    overlay->addSubview(content);
    return overlay;
}

std::shared_ptr<PandaUI::Panel> NeverlandHUD::makeMovePad() {
    auto pad = std::make_shared<PandaUI::Panel>();
    pad->setBackgroundColor(transparent());
    pad->layoutSetAbsolute();
    pad->layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(NeverlandHUDLayout::MovePadLeftMargin));
    pad->layout().setPosition(PandaUI::Edge::Bottom, PandaUI::Length::points(NeverlandHUDLayout::MovePadBottom));
    pad->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::MovePadSize));
    pad->layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::MovePadSize));

    auto forward = makeTouchControlButton("W", NeverlandTouchControls::Button::Forward);
    forward->layoutSetAbsolute();
    forward->layout().setPosition(
        PandaUI::Edge::Left,
        PandaUI::Length::points(NeverlandHUDLayout::TouchButtonSize + NeverlandHUDLayout::TouchButtonGap)
    );
    forward->layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(0.f));
    pad->addSubview(forward);

    auto left = makeTouchControlButton("A", NeverlandTouchControls::Button::Left);
    left->layoutSetAbsolute();
    left->layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(0.f));
    left->layout().setPosition(
        PandaUI::Edge::Top,
        PandaUI::Length::points(NeverlandHUDLayout::TouchButtonSize + NeverlandHUDLayout::TouchButtonGap)
    );
    pad->addSubview(left);

    auto backward = makeTouchControlButton("S", NeverlandTouchControls::Button::Backward);
    backward->layoutSetAbsolute();
    backward->layout().setPosition(
        PandaUI::Edge::Left,
        PandaUI::Length::points(NeverlandHUDLayout::TouchButtonSize + NeverlandHUDLayout::TouchButtonGap)
    );
    backward->layout().setPosition(
        PandaUI::Edge::Top,
        PandaUI::Length::points((NeverlandHUDLayout::TouchButtonSize + NeverlandHUDLayout::TouchButtonGap) * 2.0f)
    );
    pad->addSubview(backward);

    auto right = makeTouchControlButton("D", NeverlandTouchControls::Button::Right);
    right->layoutSetAbsolute();
    right->layout().setPosition(
        PandaUI::Edge::Left,
        PandaUI::Length::points((NeverlandHUDLayout::TouchButtonSize + NeverlandHUDLayout::TouchButtonGap) * 2.0f)
    );
    right->layout().setPosition(
        PandaUI::Edge::Top,
        PandaUI::Length::points(NeverlandHUDLayout::TouchButtonSize + NeverlandHUDLayout::TouchButtonGap)
    );
    pad->addSubview(right);
    return pad;
}

std::shared_ptr<PandaUI::Panel> NeverlandHUD::makeActionControls() {
    auto panel = std::make_shared<PandaUI::Panel>();
    panel->setBackgroundColor(transparent());
    panel->layoutSetAbsolute();
    panel->layout().setPosition(PandaUI::Edge::Right, PandaUI::Length::points(NeverlandHUDLayout::ActionControlsMargin));
    panel->layout().setPosition(PandaUI::Edge::Bottom, PandaUI::Length::points(NeverlandHUDLayout::ActionControlsBottom));
    panel->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::JumpButtonWidth));
    panel->layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::JumpButtonHeight));

    auto jump = std::make_shared<TouchControlButton>("JUMP");
    jump->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::JumpButtonWidth));
    jump->layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::JumpButtonHeight));
    jump->layout().setPadding(0.f);
    jump->setFont(PandaUI::Font(13.f));
    jump->setOnPressedChanged([](bool pressed) {
        NeverlandTouchControls::setPressed(NeverlandTouchControls::Button::Jump, pressed);
    });
    panel->addSubview(jump);
    return panel;
}

std::shared_ptr<PandaUI::Panel> NeverlandHUD::makeHotbar() {
    auto hotbar = std::make_shared<PandaUI::Panel>();
    hotbar->setBackgroundColor(PandaUI::Color(0x111319CC));
    hotbar->setClipsToBounds(true);
    hotbar->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::HotbarWidth));
    hotbar->layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::HotbarHeight));
    hotbar->layout().setMargin(PandaUI::Edge::Bottom, NeverlandHUDLayout::HotbarBottomMargin);
    hotbar->layout().setPadding(PandaUI::Edge::Horizontal, NeverlandHUDLayout::HotbarPaddingHorizontal);
    hotbar->layout().setPadding(PandaUI::Edge::Vertical, NeverlandHUDLayout::HotbarPaddingVertical);
    hotbar->layout().setGap(NeverlandHUDLayout::HotbarGap);
    hotbar->layout().setFlexDirection(PandaUI::FlexDirection::Row);

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
    view->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::HotbarSlotWidth));
    view->layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::HotbarSlotHeight));
    view->layout().setPadding(NeverlandHUDLayout::HotbarSlotPadding);
    view->layout().setGap(NeverlandHUDLayout::HotbarSlotGap);
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
    color->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::HotbarSwatchWidth));
    color->layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::HotbarSwatchHeight));
    view->addSubview(color);

    const std::string keyLabel = index == 9 ? "0" : std::to_string(index + 1);
    auto label = makeLabel(keyLabel, NeverlandHUDLayout::HotbarLabelFontSize, PandaUI::Color(0xE9EDF4FF));
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
