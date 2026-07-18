#include "TerrainBrushPanel.hpp"

#include <string>
#include <utility>

namespace {

constexpr float PANEL_WIDTH = 108.f;
constexpr float BUTTON_HEIGHT = 30.f;
constexpr float SMALL_BUTTON_WIDTH = 26.f;

} // namespace

// Кнопка панели кисти: стиль HUD (тёмная подложка, выбранная — светлая).
class BrushPanelButton final : public PandaUI::Button {
public:
    explicit BrushPanelButton(std::string text)
        : PandaUI::Button(std::move(text)) {
        setFocusable(false);
        setFont(PandaUI::Font(13.f));
        surface().setCornerRadius(8.f);
        surface().setBorderWidth(1.f);
        layout().setHeight(PandaUI::Length::points(BUTTON_HEIGHT));
        layout().setPadding(0.f);
        updateAppearance();
    }

    void setSelected(bool selected) {
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
            setBackgroundColor(PandaUI::Color(0xF4F7FFFF));
            getTitleLabel()->setTextColor(PandaUI::Color(0x111722FF));
            surface().setBorderColor(PandaUI::Color(0xFFFFFFFF));
        } else if (m_selected) {
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

    bool m_selected = false;
};

TerrainBrushPanel::TerrainBrushPanel(
    std::function<void(TerrainBrushMode)> onModeChanged, std::function<void(int)> onSizeChanged
)
    : m_onModeChanged(std::move(onModeChanged))
    , m_onSizeChanged(std::move(onSizeChanged)) {
    setBackgroundColor(PandaUI::Color(0x111319CC)); // подложка как у хотбара
    surface().setCornerRadius(12.f);
    surface().setBorderWidth(1.f);
    surface().setBorderColor(PandaUI::Color(0xFFFFFF33));
    layout().setFlexDirection(PandaUI::FlexDirection::Column);
    layout().setPadding(8.f);
    layout().setGap(6.f);
    layout().setWidth(PandaUI::Length::points(PANEL_WIDTH));

    auto title = std::make_shared<PandaUI::Label>("Brush (B)");
    title->setFont(PandaUI::Font(12.f));
    title->setTextColor(PandaUI::Color(0x9AA6BAFF));
    addSubview(title);

    const TerrainBrushMode modes[3] = {
        TerrainBrushMode::Sphere, TerrainBrushMode::Raise, TerrainBrushMode::Flatten
    };
    for (int index = 0; index < 3; index++) {
        const TerrainBrushMode mode = modes[index];
        auto button = std::make_shared<BrushPanelButton>(TerrainBrush::modeName(mode));
        button->layout().setWidth(PandaUI::Length::percent(100.f));
        button->setOnClick([this, mode](PandaUI::Button &) {
            if (m_onModeChanged) { m_onModeChanged(mode); }
        });
        m_modeButtons[index] = button;
        addSubview(button);
    }

    // Размер: [-] N/M [+]
    auto sizeRow = std::make_shared<PandaUI::Panel>();
    sizeRow->setBackgroundColor(PandaUI::Color(0x00000000));
    sizeRow->layout().setFlexDirection(PandaUI::FlexDirection::Row);
    sizeRow->layout().setAlignItems(PandaUI::Align::Center);
    sizeRow->layout().setGap(6.f);
    sizeRow->layout().setWidth(PandaUI::Length::percent(100.f));

    auto minusButton = std::make_shared<BrushPanelButton>("-");
    minusButton->layout().setWidth(PandaUI::Length::points(SMALL_BUTTON_WIDTH));
    minusButton->setOnClick([this](PandaUI::Button &) {
        if (m_onSizeChanged) { m_onSizeChanged(m_size - 1); }
    });

    auto sizeCenter = std::make_shared<PandaUI::Panel>();
    sizeCenter->setBackgroundColor(PandaUI::Color(0x00000000));
    sizeCenter->layout().setFlexGrow(1.f);
    sizeCenter->layout().setAlignItems(PandaUI::Align::Center);
    sizeCenter->layout().setJustifyContent(PandaUI::Justify::Center);
    m_sizeLabel = std::make_shared<PandaUI::Label>("");
    m_sizeLabel->setFont(PandaUI::Font(13.f));
    m_sizeLabel->setTextColor(PandaUI::Color(0xF4F7FFFF));
    sizeCenter->addSubview(m_sizeLabel);

    auto plusButton = std::make_shared<BrushPanelButton>("+");
    plusButton->layout().setWidth(PandaUI::Length::points(SMALL_BUTTON_WIDTH));
    plusButton->setOnClick([this](PandaUI::Button &) {
        if (m_onSizeChanged) { m_onSizeChanged(m_size + 1); }
    });

    sizeRow->addSubview(minusButton);
    sizeRow->addSubview(sizeCenter);
    sizeRow->addSubview(plusButton);
    addSubview(sizeRow);

    auto hint = std::make_shared<PandaUI::Label>("Alt — cursor");
    hint->setFont(PandaUI::Font(10.f));
    hint->setTextColor(PandaUI::Color(0x9AA6BAFF));
    addSubview(hint);

    applySelection();
}

void TerrainBrushPanel::setState(TerrainBrushMode mode, int size, int sizeCount) {
    if (m_mode == mode && m_size == size && m_sizeCount == sizeCount) { return; }
    m_mode = mode;
    m_size = size;
    m_sizeCount = sizeCount;
    applySelection();
}

void TerrainBrushPanel::applySelection() {
    const TerrainBrushMode modes[3] = {
        TerrainBrushMode::Sphere, TerrainBrushMode::Raise, TerrainBrushMode::Flatten
    };
    for (int index = 0; index < 3; index++) {
        if (m_modeButtons[index]) { m_modeButtons[index]->setSelected(modes[index] == m_mode); }
    }
    if (m_sizeLabel) {
        m_sizeLabel->setText(std::to_string(m_size + 1) + "/" + std::to_string(m_sizeCount));
    }
}
