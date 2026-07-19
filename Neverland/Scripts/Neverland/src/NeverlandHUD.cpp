#include "NeverlandHUD.hpp"
#include "Model/Voxel.hpp"
#include "Model/VoxelTextureMapper.hpp"
#include "NeverlandTouchControls.hpp"

#include <Bamboo/ApplicationAPI.hpp>
#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/Input.hpp>
#include <Bamboo/Logger.hpp>
#include <Bamboo/Assets/TextureAPI.hpp>
#include <Bamboo/UI/TextureAPI.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace {

PandaUI::Color transparent() {
    return PandaUI::Color(0x00000000);
}

struct Rgba {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
};

struct TexturePixels {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<unsigned char> pixels;
};

constexpr uint32_t PREVIEW_SIZE = 32;

Rgba colorFromHex(uint32_t hex) {
    return {
        static_cast<uint8_t>((hex >> 24) & 0xFF),
        static_cast<uint8_t>((hex >> 16) & 0xFF),
        static_cast<uint8_t>((hex >> 8) & 0xFF),
        static_cast<uint8_t>(hex & 0xFF)
    };
}

Rgba multiplyColor(Rgba color, uint32_t tintHex) {
    const Rgba tint = colorFromHex(tintHex);
    const auto multiply = [](uint8_t value, uint8_t tintValue) {
        return static_cast<uint8_t>(static_cast<int>(value) * static_cast<int>(tintValue) / 255);
    };
    color.r = multiply(color.r, tint.r);
    color.g = multiply(color.g, tint.g);
    color.b = multiply(color.b, tint.b);
    color.a = static_cast<uint8_t>(
        std::clamp(static_cast<int>(color.a) * static_cast<int>(tint.a) / 255, 0, 255)
    );
    return color;
}

Rgba sampleTile(
    const TexturePixels &atlas, int atlasGrid, uint8_t tileIndex, uint32_t x, uint32_t y, uint32_t tintHex
) {
    // Тайл может быть нецелым в пикселях (1254 / 7) — считаем координаты во float.
    const float tileSize = static_cast<float>(atlas.width) / atlasGrid;
    if (tileSize < 1.f || atlas.height < static_cast<uint32_t>(tileSize)) { return {}; }

    const float tileX = static_cast<float>(tileIndex % atlasGrid) * tileSize;
    const float tileY = static_cast<float>(tileIndex / atlasGrid) * tileSize;
    const uint32_t sourceX =
        std::min(static_cast<uint32_t>(tileX + x * tileSize / PREVIEW_SIZE), atlas.width - 1);
    const uint32_t sourceY =
        std::min(static_cast<uint32_t>(tileY + y * tileSize / PREVIEW_SIZE), atlas.height - 1);
    const size_t index = (static_cast<size_t>(sourceY) * atlas.width + sourceX) * 4;
    if (index + 3 >= atlas.pixels.size()) { return {}; }

    Rgba color{atlas.pixels[index], atlas.pixels[index + 1], atlas.pixels[index + 2],
               atlas.pixels[index + 3]};
    return multiplyColor(color, tintHex);
}

struct PreviewTile {
    uint8_t index = 0;
    uint32_t tint = 0xFFFFFFFF;
};

PreviewTile getPreviewTile(VoxelType type, const VoxelTextureData &texture) {
    if (type == VoxelType::GRASS) { return {texture.topTileIndex, texture.topColor}; }
    return {texture.sideTileIndex, texture.sideColor};
}

PandaUI::TextureHandle makeTexturePreview(const TexturePixels &atlas, int atlasGrid, VoxelType type) {
    Voxel voxel(type);
    const VoxelTextureData &texture = VoxelTextureMapper::getTextureData(&voxel);
    const PreviewTile previewTile = getPreviewTile(type, texture);
    std::vector<unsigned char> pixels(PREVIEW_SIZE * PREVIEW_SIZE * 4, 0);

    for (uint32_t y = 0; y < PREVIEW_SIZE; ++y) {
        for (uint32_t x = 0; x < PREVIEW_SIZE; ++x) {
            const Rgba color = sampleTile(atlas, atlasGrid, previewTile.index, x, y, previewTile.tint);
            const size_t index = (static_cast<size_t>(y) * PREVIEW_SIZE + x) * 4;
            pixels[index] = color.r;
            pixels[index + 1] = color.g;
            pixels[index + 2] = color.b;
            pixels[index + 3] = color.a;
        }
    }

    return Bamboo::UI::createTextureRGBA8(
        pixels.data(), static_cast<uint16_t>(PREVIEW_SIZE), static_cast<uint16_t>(PREVIEW_SIZE)
    );
}

} // namespace

// Карточка меню блоков: превью (или крупная подпись у элементов) + имя, подсветка выбора.
class BlockCardButton final : public PandaUI::Button {
public:
    BlockCardButton(const char *name, PandaUI::TextureHandle preview, const char *fallbackText)
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

    void setCardSelected(bool selected) {
        if (m_selected == selected) { return; }
        m_selected = selected;
        updateAppearance();
    }

private:
    void controlStateChanged(PandaUI::ControlState, PandaUI::ControlState) override {
        updateAppearance();
    }

    void themeChanged() override {
        updateAppearance();
    }

    void updateAppearance() {
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

    bool m_selected;
};

namespace {

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

// Панель кисти terrain — только desktop (клики по ней требуют свободного курсора по Alt).
bool shouldShowBrushPanel() {
    return !shouldShowTouchControls();
}

std::shared_ptr<PandaUI::Label>
makeLabel(const std::string &text, float fontSize, PandaUI::Color color) {
    auto label = std::make_shared<PandaUI::Label>(text);
    label->setFont(PandaUI::Font(fontSize));
    label->setTextColor(color);
    return label;
}

std::shared_ptr<PandaUI::Panel> makeBlockPreview(PandaUI::TextureHandle texture) {
    auto preview = std::make_shared<PandaUI::Panel>();
    preview->setBackgroundColor(transparent());
    preview->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::HotbarSwatchWidth));
    preview->layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::HotbarSwatchHeight));

    if (texture) {
        auto image = std::make_shared<PandaUI::ImageView>(texture);
        image->setContentMode(PandaUI::ImageContentMode::Fit);
        image->setUserInteractionEnabled(false);
        image->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::HotbarSwatchWidth));
        image->layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::HotbarSwatchHeight));
        preview->addSubview(image);
    }

    return preview;
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
        if (event.type == PandaUI::PointerType::Touch) {
            NeverlandTouchControls::ignoreTouch(event.pointerId);
        }
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
            setBackgroundColor(
                m_selected ? PandaUI::Color(0xFFFFFFFF) : PandaUI::Color(0x414A59EE)
            );
        } else {
            setBackgroundColor(
                m_selected ? PandaUI::Color(0xF0F4FFFF) : PandaUI::Color(0x2D3440DD)
            );
        }
        surface().setBorderColor(
            m_selected ? PandaUI::Color(0xFFFFFFFF) : PandaUI::Color(0x5C6676AA)
        );
        surface().setShadowColor(
            m_selected ? PandaUI::Color(0x00000038) : PandaUI::Color(0x00000024)
        );
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
        if (event.type == PandaUI::PointerType::Touch) {
            NeverlandTouchControls::ignoreTouch(event.pointerId);
        }
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

// Кнопка меню — стиль TouchControlButton, но крупнее и с текстом по центру.
class MenuButton final : public PandaUI::Button {
public:
    explicit MenuButton(std::string text)
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

private:
    void controlStateChanged(PandaUI::ControlState, PandaUI::ControlState) override {
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
};

std::shared_ptr<MenuButton> makeMenuButton(const std::string &text, std::function<void()> action) {
    auto button = std::make_shared<MenuButton>(text);
    button->setOnClick([action = std::move(action)](PandaUI::Button &) {
        if (action) { action(); }
    });
    return button;
}

// Полноэкранное затемнение с колонкой контента по центру (общий каркас обоих меню).
std::shared_ptr<PandaUI::Panel> makeMenuOverlay(PandaUI::Color dim) {
    auto overlay = std::make_shared<PandaUI::Panel>();
    overlay->setBackgroundColor(dim);
    overlay->layoutSetAbsolute();
    overlay->layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(0.f));
    overlay->layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(0.f));
    overlay->layout().setWidth(PandaUI::Length::percent(100.f));
    overlay->layout().setHeight(PandaUI::Length::percent(100.f));
    overlay->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    overlay->layout().setJustifyContent(PandaUI::Justify::Center);
    overlay->layout().setAlignItems(PandaUI::Align::Center);
    overlay->layout().setGap(NeverlandHUDLayout::MenuButtonGap);
    return overlay;
}

} // namespace

void NeverlandHUD::start() {
    NeverlandTouchControls::reset();
    GameMenu::reset(); // старт мира — главное меню, курсор свободен
    m_blocksCreation = EntityAPI::getScript<BlocksCreation>(getEntity());
    buildUI();
    updateTouchControlSafeArea();
    updateSelection();
    m_appliedMenuState = GameMenuState::Playing; // форс первого applyMenuState (state = MainMenu)
    applyMenuState();
}

void NeverlandHUD::update(float) {
    updateTouchControlSafeArea();
    updateSelection();
    updateMenuInput();
    if (shouldShowBrushPanel()) { updateBrushPanel(); }
    applyMenuState();
    if (shouldShowTouchControls()) { updateMoveStickOverlay(); }
}

void NeverlandHUD::shutdown() {
    NeverlandTouchControls::reset();
    GameMenu::reset();
    if (m_window.isValid()) { m_window.setRootView(nullptr); }
    destroyBlockPreviewTextures();
    m_slots.fill(nullptr);
    m_slotPreviews.fill(nullptr);
    m_displayedHotbar.clear();
    m_selectionLabel.reset();
    m_stickBase.reset();
    m_stickKnob.reset();
    m_brushPanel.reset();
    m_blockCards.clear();
    m_elementCards.clear();
    m_hudLayer.reset();
    m_mainMenu.reset();
    m_pauseMenu.reset();
    m_blocksMenu.reset();
    m_root.reset();
    m_blocksCreation.reset();
    m_displayedSelection = VoxelType::NOTHING;
}

void NeverlandHUD::updateMenuInput() {
    if (Input::isKeyJustPressed(Key::M)) {
        if (GameMenu::state() == GameMenuState::Playing) {
            GameMenu::setState(GameMenuState::BlockPicker);
        } else if (GameMenu::state() == GameMenuState::BlockPicker) {
            GameMenu::setState(GameMenuState::Playing);
        }
    }
    if (!Input::isKeyJustPressed(Key::ESCAPE)) { return; }
    if (GameMenu::state() == GameMenuState::Playing) {
        GameMenu::setState(GameMenuState::Paused);
    } else if (GameMenu::state() == GameMenuState::Paused ||
               GameMenu::state() == GameMenuState::BlockPicker) {
        GameMenu::setState(GameMenuState::Playing);
    }
}

void NeverlandHUD::updateBrushPanel() {
    GameMenu::setUIModifierHeld(
        GameMenu::state() == GameMenuState::Playing && Input::isKeyPressed(Key::LEFT_ALT)
    );
    if (!m_brushPanel || !m_blocksCreation) { return; }
    // Панель — постоянный инструмент рельефа; подсветка материала горит в терраформ-режиме.
    const bool terraformActive = !m_blocksCreation->isElementSelected() &&
                                 isNaturalVoxelType(m_blocksCreation->getSelectedBlock());
    m_brushPanel->setState(
        m_blocksCreation->getBrushMode(), m_blocksCreation->getBrushSize(),
        BlocksCreation::brushSizeCount(), m_blocksCreation->getSelectedBlock(), terraformActive
    );
}

void NeverlandHUD::applyMenuState() {
    const GameMenuState state = GameMenu::state();
    if (state == m_appliedMenuState) { return; }
    m_appliedMenuState = state;
    if (m_hudLayer) { m_hudLayer->setHidden(state != GameMenuState::Playing); }
    if (m_mainMenu) { m_mainMenu->setHidden(state != GameMenuState::MainMenu); }
    if (m_pauseMenu) { m_pauseMenu->setHidden(state != GameMenuState::Paused); }
    if (m_blocksMenu) { m_blocksMenu->setHidden(state != GameMenuState::BlockPicker); }
}

void NeverlandHUD::buildUI() {
    m_window = PandaUI::Window::main();
    if (!m_window.isValid()) {
        LOG_ERROR("NeverlandHUD: main PandaUI window is unavailable");
        return;
    }
    loadBlockPreviewTextures();

    m_root = std::make_shared<PandaUI::Panel>();
    m_root->setBackgroundColor(transparent());
    m_root->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    m_root->layout().setWidth(PandaUI::Length::percent(100.f));
    m_root->layout().setHeight(PandaUI::Length::percent(100.f));

    // Игровой HUD — отдельный слой, прячется в меню.
    m_hudLayer = std::make_shared<PandaUI::Panel>();
    m_hudLayer->setBackgroundColor(transparent());
    m_hudLayer->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    m_hudLayer->layout().setWidth(PandaUI::Length::percent(100.f));
    m_hudLayer->layout().setHeight(PandaUI::Length::percent(100.f));

    auto safeArea = std::make_shared<PandaUI::SafeAreaView>();
    safeArea->setBackgroundColor(transparent());
    safeArea->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    safeArea->layout().setJustifyContent(PandaUI::Justify::End);
    safeArea->layout().setAlignItems(PandaUI::Align::Center);
    safeArea->addSubview(makeSelectionPill());
    safeArea->addSubview(makeHotbar());

    if (shouldShowCrosshair()) { m_hudLayer->addSubview(makeCrosshair()); }
    m_hudLayer->addSubview(safeArea);
    if (shouldShowTouchControls()) { m_hudLayer->addSubview(makeTouchControls()); }
    if (shouldShowBrushPanel()) {
        const std::array<PandaUI::TextureHandle, 4> materialPreviews = {
            previewFor(VoxelType::GRASS), previewFor(VoxelType::GROUND),
            previewFor(VoxelType::STONE), previewFor(VoxelType::SAND)
        };
        m_brushPanel = std::make_shared<TerrainBrushPanel>(
            [this](VoxelType material) {
                if (m_blocksCreation) { m_blocksCreation->setSelectedBlock(material); }
            },
            [this](GameBrushMode mode) {
                if (m_blocksCreation) { m_blocksCreation->setBrushMode(mode); }
            },
            [this](int size) {
                if (m_blocksCreation) { m_blocksCreation->setBrushSize(size); }
            },
            materialPreviews
        );
        m_brushPanel->layoutSetAbsolute();
        m_brushPanel->layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(16.f));
        m_brushPanel->layout().setPosition(PandaUI::Edge::Bottom, PandaUI::Length::points(16.f));
        m_hudLayer->addSubview(m_brushPanel);
    }

    m_mainMenu = makeMainMenu();
    m_pauseMenu = makePauseMenu();
    m_blocksMenu = makeBlocksMenu();
    m_root->addSubview(m_hudLayer);
    m_root->addSubview(m_mainMenu);
    m_root->addSubview(m_pauseMenu);
    m_root->addSubview(m_blocksMenu);
    m_window.setRootView(m_root);
}

// Меню выбора блоков: строительные блоки и элементы отдельно от материалов рельефа.
std::shared_ptr<PandaUI::Panel> NeverlandHUD::makeBlocksMenu() {
    m_blockCards.clear();
    m_elementCards.clear();
    auto overlay = makeMenuOverlay(PandaUI::Color(0x0B101AA8));

    auto card = std::make_shared<PandaUI::Panel>();
    card->setBackgroundColor(PandaUI::Color(0x151B26F2));
    card->surface().setCornerRadius(18.f);
    card->surface().setBorderWidth(1.f);
    card->surface().setBorderColor(PandaUI::Color(0xFFFFFF2E));
    card->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    card->layout().setPadding(20.f);
    card->layout().setGap(10.f);

    const auto sectionLabel = [](const char *text) {
        auto label = std::make_shared<PandaUI::Label>(text);
        label->setFont(PandaUI::Font(13.f));
        label->setTextColor(PandaUI::Color(0x9AA6BAFF));
        return label;
    };
    const auto cardsRow = [] {
        auto row = std::make_shared<PandaUI::Panel>();
        row->setBackgroundColor(PandaUI::Color(0x00000000));
        row->layout().setFlexDirection(PandaUI::FlexDirection::Row);
        row->layout().setGap(8.f);
        return row;
    };
    const auto divider = [] {
        auto line = std::make_shared<PandaUI::Panel>();
        line->setBackgroundColor(PandaUI::Color(0xFFFFFF1E));
        line->layout().setWidth(PandaUI::Length::percent(100.f));
        line->layout().setHeight(PandaUI::Length::points(1.f));
        line->layout().setMargin(PandaUI::Edge::Vertical, 4.f);
        return line;
    };

    auto titleRow = std::make_shared<PandaUI::Panel>();
    titleRow->setBackgroundColor(PandaUI::Color(0x00000000));
    titleRow->layout().setFlexDirection(PandaUI::FlexDirection::Row);
    titleRow->layout().setAlignItems(PandaUI::Align::Center);
    titleRow->layout().setJustifyContent(PandaUI::Justify::SpaceBetween);
    titleRow->layout().setWidth(PandaUI::Length::percent(100.f));
    auto title = std::make_shared<PandaUI::Label>("Blocks");
    title->setFont(PandaUI::Font(24.f));
    title->setTextColor(PandaUI::Color(0xF4F7FFFF));
    titleRow->addSubview(title);
    auto closeButton = std::make_shared<MenuButton>("Close");
    closeButton->layout().setWidth(PandaUI::Length::points(96.f));
    closeButton->layout().setHeight(PandaUI::Length::points(34.f));
    closeButton->setFont(PandaUI::Font(14.f));
    closeButton->setOnClick([](PandaUI::Button &) {
        GameMenu::setState(GameMenuState::Playing);
    });
    titleRow->addSubview(closeButton);
    card->addSubview(titleRow);

    // Building: сетка фиксированными рядами (PandaUI без flex-wrap).
    card->addSubview(sectionLabel("Building"));
    constexpr size_t CARDS_PER_ROW = 4;
    std::shared_ptr<PandaUI::Panel> row;
    for (size_t i = 0; i < BlockPalette::BUILDING_BLOCKS.size(); ++i) {
        if (i % CARDS_PER_ROW == 0) {
            row = cardsRow();
            card->addSubview(row);
        }
        const BlockPalette::BlockEntry &entry = BlockPalette::BUILDING_BLOCKS[i];
        auto blockCard =
            std::make_shared<BlockCardButton>(entry.name, previewFor(entry.type), nullptr);
        blockCard->setOnClick([this, type = entry.type](PandaUI::Button &) {
            if (m_blocksCreation) { m_blocksCreation->setSelectedBlock(type); }
        });
        m_blockCards.emplace_back(entry.type, blockCard);
        row->addSubview(blockCard);
    }

    card->addSubview(sectionLabel("Elements"));
    row = cardsRow();
    for (const BlockPalette::ElementEntry &entry : BlockPalette::ELEMENTS) {
        auto elementCard = std::make_shared<BlockCardButton>(entry.name, PandaUI::TextureHandle{}, entry.hint);
        elementCard->setOnClick([this, type = entry.type](PandaUI::Button &) {
            if (m_blocksCreation) { m_blocksCreation->setSelectedElement(type); }
        });
        m_elementCards.emplace_back(entry.type, elementCard);
        row->addSubview(elementCard);
    }
    card->addSubview(row);

    // Terrain — отдельная секция: материалы рельефа не смешиваются со строительными.
    card->addSubview(divider());
    card->addSubview(sectionLabel("Terrain — sculpting"));
    row = cardsRow();
    for (const BlockPalette::BlockEntry &entry : BlockPalette::TERRAIN_MATERIALS) {
        auto terrainCard =
            std::make_shared<BlockCardButton>(entry.name, previewFor(entry.type), nullptr);
        terrainCard->setOnClick([this, type = entry.type](PandaUI::Button &) {
            if (m_blocksCreation) { m_blocksCreation->setSelectedBlock(type); }
        });
        m_blockCards.emplace_back(entry.type, terrainCard);
        row->addSubview(terrainCard);
    }
    card->addSubview(row);

    auto hint = std::make_shared<PandaUI::Label>("M / Esc — close");
    hint->setFont(PandaUI::Font(11.f));
    hint->setTextColor(PandaUI::Color(0x9AA6BAFF));
    card->addSubview(hint);

    overlay->addSubview(card);
    return overlay;
}

std::shared_ptr<PandaUI::Panel> NeverlandHUD::makeMainMenu() {
    auto overlay = makeMenuOverlay(PandaUI::Color(0x0B101AC8));

    auto title = makeLabel("NEVERLAND", NeverlandHUDLayout::MenuTitleFontSize, PandaUI::Color(0xF4F7FFFF));
    auto subtitle =
        makeLabel("Voxel sandbox on Panda", NeverlandHUDLayout::MenuSubtitleFontSize, PandaUI::Color(0xB8C2D4FF));
    subtitle->layout().setMargin(PandaUI::Edge::Bottom, NeverlandHUDLayout::MenuTitleGap);

    overlay->addSubview(title);
    overlay->addSubview(subtitle);
    overlay->addSubview(makeMenuButton("Play", [] { GameMenu::setState(GameMenuState::Playing); }));
    overlay->addSubview(makeMenuButton("Quit", [] { ApplicationAPI::quit(); }));
    return overlay;
}

std::shared_ptr<PandaUI::Panel> NeverlandHUD::makePauseMenu() {
    auto overlay = makeMenuOverlay(PandaUI::Color(0x0B101A90));

    auto title = makeLabel("Paused", 34.f, PandaUI::Color(0xF4F7FFFF));
    title->layout().setMargin(PandaUI::Edge::Bottom, NeverlandHUDLayout::MenuTitleGap);

    overlay->addSubview(title);
    overlay->addSubview(makeMenuButton("Resume", [] { GameMenu::setState(GameMenuState::Playing); }));
    overlay->addSubview(makeMenuButton("Quit", [] { ApplicationAPI::quit(); }));
    return overlay;
}

void NeverlandHUD::loadBlockPreviewTextures() {
    destroyBlockPreviewTextures();

    // Два атласа: природные превью — из ground (6×6), рукотворные — из materials (7×7).
    auto readAtlas = [](TextureHandle handle, TexturePixels &out) {
        const Bamboo::TextureAPI::TexturePixelsRGBA8 texture = Bamboo::TextureAPI::readPixelsRGBA8(handle);
        if (!texture) {
            LOG_ERROR("NeverlandHUD: failed to read preview texture asset %u", handle.id);
            return false;
        }
        out.width = texture.width;
        out.height = texture.height;
        out.pixels = texture.pixels;
        return true;
    };
    TexturePixels blocksAtlas, groundAtlas;
    const bool hasBlocks = readAtlas(blocksTileTexture, blocksAtlas);
    const bool hasGround = readAtlas(groundTileTexture, groundAtlas);

    const auto store = [this](VoxelType type, PandaUI::TextureHandle texture) {
        m_previewTextures[static_cast<size_t>(type)] = texture;
    };
    if (hasBlocks) {
        for (const BlockPalette::BlockEntry &entry : BlockPalette::BUILDING_BLOCKS) {
            store(entry.type, makeTexturePreview(blocksAtlas, 7, entry.type));
        }
    }
    if (hasGround) {
        for (const BlockPalette::BlockEntry &entry : BlockPalette::TERRAIN_MATERIALS) {
            store(entry.type, makeTexturePreview(groundAtlas, 6, entry.type));
        }
    }
}

PandaUI::TextureHandle NeverlandHUD::previewFor(VoxelType type) const {
    return m_previewTextures[static_cast<size_t>(type)];
}

void NeverlandHUD::destroyBlockPreviewTextures() {
    for (PandaUI::TextureHandle &texture : m_previewTextures) {
        if (texture) {
            Bamboo::UI::destroyTexture(texture);
            texture = {};
        }
    }
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
    horizontal->layout().setPosition(
        PandaUI::Edge::Left, PandaUI::Length::points(NeverlandHUDLayout::CrosshairLineInset)
    );
    horizontal->layout().setPosition(
        PandaUI::Edge::Top, PandaUI::Length::points(NeverlandHUDLayout::CrosshairLineCenter)
    );
    horizontal->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::CrosshairLineLength));
    horizontal->layout().setHeight(
        PandaUI::Length::points(NeverlandHUDLayout::CrosshairLineThickness)
    );

    auto vertical = std::make_shared<PandaUI::Panel>();
    vertical->setBackgroundColor(PandaUI::Color(0xFFFFFFFF));
    vertical->layoutSetAbsolute();
    vertical->layout().setPosition(
        PandaUI::Edge::Left, PandaUI::Length::points(NeverlandHUDLayout::CrosshairLineCenter)
    );
    vertical->layout().setPosition(
        PandaUI::Edge::Top, PandaUI::Length::points(NeverlandHUDLayout::CrosshairLineInset)
    );
    vertical->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::CrosshairLineThickness)
    );
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
    content->addSubview(makeMoveStickOverlay());
    content->addSubview(makeActionControls());
    overlay->addSubview(content);
    return overlay;
}

// Плавающий джойстик: два неинтерактивных круга, позиционируются по стейту стика
// (касания трекает PlayerController, HUD только рисует).
std::shared_ptr<PandaUI::Panel> NeverlandHUD::makeMoveStickOverlay() {
    auto overlay = std::make_shared<PandaUI::Panel>();
    overlay->setBackgroundColor(transparent());
    overlay->setUserInteractionEnabled(false);
    overlay->layoutSetAbsolute();
    overlay->layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(0.f));
    overlay->layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(0.f));
    overlay->layout().setWidth(PandaUI::Length::percent(100.f));
    overlay->layout().setHeight(PandaUI::Length::percent(100.f));

    m_stickBase = std::make_shared<PandaUI::Panel>();
    m_stickBase->setBackgroundColor(PandaUI::Color(0x1A212E55));
    m_stickBase->surface().setBorderWidth(1.5f);
    m_stickBase->surface().setBorderColor(PandaUI::Color(0xFFFFFF55));
    m_stickBase->surface().setCornerRadius(NeverlandHUDLayout::JoystickRadius);
    m_stickBase->setUserInteractionEnabled(false);
    m_stickBase->layoutSetAbsolute();
    m_stickBase->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::JoystickRadius * 2.f));
    m_stickBase->layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::JoystickRadius * 2.f));
    m_stickBase->setHidden(true);

    m_stickKnob = std::make_shared<PandaUI::Panel>();
    m_stickKnob->setBackgroundColor(PandaUI::Color(0xE8EEF8AA));
    m_stickKnob->surface().setCornerRadius(NeverlandHUDLayout::JoystickKnobRadius);
    m_stickKnob->setUserInteractionEnabled(false);
    m_stickKnob->layoutSetAbsolute();
    m_stickKnob->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::JoystickKnobRadius * 2.f));
    m_stickKnob->layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::JoystickKnobRadius * 2.f));
    m_stickKnob->setHidden(true);

    overlay->addSubview(m_stickBase);
    overlay->addSubview(m_stickKnob);
    return overlay;
}

void NeverlandHUD::updateMoveStickOverlay() {
    if (!m_stickBase || !m_stickKnob) { return; }
    const NeverlandTouchControls::MoveStick &stick = NeverlandTouchControls::getMoveStick();
    m_stickBase->setHidden(!stick.active);
    m_stickKnob->setHidden(!stick.active);
    if (!stick.active) { return; }

    m_stickBase->layout().setPosition(
        PandaUI::Edge::Left,
        PandaUI::Length::points(stick.originX - NeverlandHUDLayout::JoystickRadius)
    );
    m_stickBase->layout().setPosition(
        PandaUI::Edge::Top, PandaUI::Length::points(stick.originY - NeverlandHUDLayout::JoystickRadius)
    );
    // Ручка не выходит за подложку: смещение клампится радиусом.
    float offsetX = stick.currentX - stick.originX;
    float offsetY = stick.currentY - stick.originY;
    const float maxOffset = NeverlandHUDLayout::JoystickRadius - NeverlandHUDLayout::JoystickKnobRadius * 0.5f;
    const float offsetLength = std::sqrt(offsetX * offsetX + offsetY * offsetY);
    if (offsetLength > maxOffset && offsetLength > 0.001f) {
        offsetX = offsetX / offsetLength * maxOffset;
        offsetY = offsetY / offsetLength * maxOffset;
    }
    m_stickKnob->layout().setPosition(
        PandaUI::Edge::Left,
        PandaUI::Length::points(stick.originX + offsetX - NeverlandHUDLayout::JoystickKnobRadius)
    );
    m_stickKnob->layout().setPosition(
        PandaUI::Edge::Top,
        PandaUI::Length::points(stick.originY + offsetY - NeverlandHUDLayout::JoystickKnobRadius)
    );
}

std::shared_ptr<PandaUI::Panel> NeverlandHUD::makeActionControls() {
    auto panel = std::make_shared<PandaUI::Panel>();
    panel->setBackgroundColor(transparent());
    panel->layoutSetAbsolute();
    panel->layout().setPosition(
        PandaUI::Edge::Right, PandaUI::Length::points(NeverlandHUDLayout::ActionControlsMargin)
    );
    panel->layout().setPosition(
        PandaUI::Edge::Bottom, PandaUI::Length::points(NeverlandHUDLayout::ActionControlsBottom)
    );
    panel->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::JumpButtonWidth));
    panel->layout().setHeight(
        PandaUI::Length::points(NeverlandHUDLayout::JumpButtonHeight * 2.f + 10.f)
    );
    panel->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    panel->layout().setGap(10.f);

    auto blocks = std::make_shared<TouchControlButton>("BLOCKS");
    blocks->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::JumpButtonWidth));
    blocks->layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::JumpButtonHeight));
    blocks->layout().setPadding(0.f);
    blocks->setFont(PandaUI::Font(13.f));
    blocks->setOnPressedChanged([](bool pressed) {
        if (pressed) { GameMenu::setState(GameMenuState::BlockPicker); }
    });
    panel->addSubview(blocks);

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

std::shared_ptr<PandaUI::Panel> NeverlandHUD::makeSelectionPill() {
    auto pill = std::make_shared<PandaUI::Panel>();
    pill->setBackgroundColor(PandaUI::Color(0x111319B8));
    pill->surface().setCornerRadius(11.0f);
    pill->surface().setBorderWidth(1.0f);
    pill->surface().setBorderColor(PandaUI::Color(0xFFFFFF33));
    pill->layout().setMargin(PandaUI::Edge::Bottom, 8.0f);
    pill->layout().setPadding(PandaUI::Edge::Horizontal, 12.0f);
    pill->layout().setPadding(PandaUI::Edge::Vertical, 5.0f);

    m_selectionLabel = makeLabel("", 13.0f, PandaUI::Color(0xF4F7FFFF));
    pill->addSubview(m_selectionLabel);
    return pill;
}

std::shared_ptr<PandaUI::Panel> NeverlandHUD::makeHotbar() {
    auto hotbar = std::make_shared<PandaUI::Panel>();
    hotbar->setBackgroundColor(PandaUI::Color(0x111319CC));
    hotbar->setClipsToBounds(true);
    hotbar->surface().setCornerRadius(NeverlandHUDLayout::HotbarCornerRadius);
    hotbar->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::HotbarWidth));
    hotbar->layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::HotbarHeight));
    hotbar->layout().setMargin(PandaUI::Edge::Bottom, NeverlandHUDLayout::HotbarBottomMargin);
    hotbar->layout().setPadding(
        PandaUI::Edge::Horizontal, NeverlandHUDLayout::HotbarPaddingHorizontal
    );
    hotbar->layout().setPadding(PandaUI::Edge::Vertical, NeverlandHUDLayout::HotbarPaddingVertical);
    hotbar->layout().setGap(NeverlandHUDLayout::HotbarGap);
    hotbar->layout().setFlexDirection(PandaUI::FlexDirection::Row);

    for (int i = 0; i < NeverlandHUDLayout::HotbarSlotCount; ++i) {
        auto slot = makeSlot(i);
        m_slots[i] = slot;
        hotbar->addSubview(slot);
    }
    return hotbar;
}

void NeverlandHUD::refreshHotbar(const std::vector<VoxelType> &recent) {
    for (size_t i = 0; i < m_slotPreviews.size(); ++i) {
        if (!m_slotPreviews[i]) { continue; }
        m_slotPreviews[i]->removeAllSubviews();
        if (i >= recent.size()) { continue; }
        auto image = std::make_shared<PandaUI::ImageView>(previewFor(recent[i]));
        image->setContentMode(PandaUI::ImageContentMode::Fit);
        image->setUserInteractionEnabled(false);
        image->layout().setWidth(PandaUI::Length::points(NeverlandHUDLayout::HotbarSwatchWidth));
        image->layout().setHeight(PandaUI::Length::points(NeverlandHUDLayout::HotbarSwatchHeight));
        m_slotPreviews[i]->addSubview(image);
    }
    m_displayedHotbar = recent;
}

std::shared_ptr<PandaUI::Button> NeverlandHUD::makeSlot(int index) {
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
    view->setOnClick([this, index](PandaUI::Button &) {
        if (!m_blocksCreation) { return; }
        const std::vector<VoxelType> &recent = m_blocksCreation->getRecentBlocks();
        if (index < static_cast<int>(recent.size())) {
            m_blocksCreation->setSelectedBlock(recent[index]);
            updateSelection();
        }
    });

    auto preview = makeBlockPreview({}); // контент заполняет refreshHotbar по MRU
    m_slotPreviews[index] = preview;
    view->addSubview(preview);

    return view;
}

void NeverlandHUD::updateSelection() {
    if (!m_blocksCreation) { return; }
    const VoxelType selected = m_blocksCreation->getSelectedBlock();
    const bool elementSelected = m_blocksCreation->isElementSelected();
    const std::vector<VoxelType> &recent = m_blocksCreation->getRecentBlocks();
    if (selected == m_displayedSelection && elementSelected == m_displayedElementSelected &&
        recent == m_displayedHotbar) {
        return;
    }
    refreshHotbar(recent);

    // Левый слот — текущий строительный материал; при терраформе хотбар не подсвечен.
    const bool buildingSelected = !isNaturalVoxelType(selected);
    for (size_t i = 0; i < m_slots.size(); ++i) {
        applySlotStyle(i, buildingSelected && i == 0 && !recent.empty());
    }
    for (auto &[type, card] : m_blockCards) {
        if (card) { card->setCardSelected(type == selected); }
    }
    // Форма и материал независимы: подсветка формы не гасится выбором блока.
    for (auto &[type, card] : m_elementCards) {
        if (card) { card->setCardSelected(type == m_blocksCreation->getSelectedElement()); }
    }
    if (m_selectionLabel) {
        std::string name;
        if (elementSelected && buildingSelected) {
            for (const BlockPalette::ElementEntry &entry : BlockPalette::ELEMENTS) {
                if (entry.type == m_blocksCreation->getSelectedElement()) { name = entry.name; }
            }
            name += std::string(" — ") + BlockPalette::nameFor(selected);
        } else {
            name = BlockPalette::nameFor(selected);
        }
        m_selectionLabel->setText(name);
    }
    m_displayedSelection = selected;
    m_displayedElementSelected = elementSelected;
}

void NeverlandHUD::applySlotStyle(size_t index, bool selected) {
    if (!m_slots[index]) { return; }
    auto slot = std::dynamic_pointer_cast<HotbarSlotButton>(m_slots[index]);
    if (!slot) { return; }
    slot->setSlotSelected(selected);
}
