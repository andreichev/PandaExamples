#include "ArchLibraryMenu.hpp"

#include "ArchesPanel.hpp"
#include "BalconiesPanel.hpp"
#include "ColumnsPanel.hpp"
#include "CornicesPanel.hpp"
#include "DecorPanel.hpp"
#include "DomesPanel.hpp"
#include "DoorsPanel.hpp"
#include "FacadesPanel.hpp"
#include "FoundationsPanel.hpp"
#include "MaterialsPanel.hpp"
#include "PilastersPanel.hpp"
#include "RoofsPanel.hpp"
#include "StairsPanel.hpp"
#include "TerrainPanel.hpp"
#include "WallsPanel.hpp"
#include "WindowsPanel.hpp"

#include "../GameMenu.hpp"
#include "../NeverlandHUDLayout.hpp"

namespace {

// Пункт сайдбара: текст слева, активный раздел подсвечен постоянно.
class SidebarButton final : public PandaUI::Button {
public:
    explicit SidebarButton(const char *text)
        : PandaUI::Button(text) {
        setFocusable(false);
        setFont(PandaUI::Font(13.f));
        surface().setCornerRadius(8.f);
        layout().setHeight(PandaUI::Length::points(26.f));
        layout().setWidth(PandaUI::Length::percent(100.f));
        layout().setPadding(PandaUI::Edge::Horizontal, 10.f);
        layout().setPadding(PandaUI::Edge::Vertical, 0.f);
        layout().setJustifyContent(PandaUI::Justify::Start);
        updateAppearance();
    }

    void setActive(bool active) {
        if (m_active == active) { return; }
        m_active = active;
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
        if (m_active || hasControlState(PandaUI::ControlState::Highlighted)) {
            setBackgroundColor(PandaUI::Color(0xE8EEF8E6));
            getTitleLabel()->setTextColor(PandaUI::Color(0x111722FF));
        } else if (hasControlState(PandaUI::ControlState::Hovered)) {
            setBackgroundColor(PandaUI::Color(0x39424FEE));
            getTitleLabel()->setTextColor(PandaUI::Color(0xF4F7FFFF));
        } else {
            setBackgroundColor(PandaUI::Color(0x00000000));
            getTitleLabel()->setTextColor(PandaUI::Color(0xC7CFDCFF));
        }
    }

    bool m_active = false;
};

} // namespace

ArchLibraryMenu::ArchLibraryMenu(
    Shared<BlocksCreation> blocks, std::function<PandaUI::TextureHandle(VoxelType)> previewFor
) {
    setBackgroundColor(PandaUI::Color(0x0B101AA8));
    layoutSetAbsolute();
    layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(0.f));
    layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(0.f));
    layout().setWidth(PandaUI::Length::percent(100.f));
    layout().setHeight(PandaUI::Length::percent(100.f));
    layout().setPadding(20.f);
    layout().setFlexDirection(PandaUI::FlexDirection::Column);

    auto card = std::make_shared<PandaUI::Panel>();
    card->setBackgroundColor(PandaUI::Color(0x151B26F2));
    card->surface().setCornerRadius(18.f);
    card->surface().setBorderWidth(1.f);
    card->surface().setBorderColor(PandaUI::Color(0xFFFFFF2E));
    card->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    card->layout().setPadding(16.f);
    card->layout().setGap(12.f);
    card->layout().setWidth(PandaUI::Length::percent(100.f));
    card->layout().setFlexGrow(1.f);

    // Шапка: заголовок + Close.
    auto header = std::make_shared<PandaUI::Panel>();
    header->setBackgroundColor(PandaUI::Color(0x00000000));
    header->layout().setFlexDirection(PandaUI::FlexDirection::Row);
    header->layout().setAlignItems(PandaUI::Align::Center);
    header->layout().setGap(10.f);
    header->layout().setWidth(PandaUI::Length::percent(100.f));

    auto title = std::make_shared<PandaUI::Label>("Architecture Library");
    title->setFont(PandaUI::Font(20.f));
    title->setTextColor(PandaUI::Color(0xF4F7FFFF));
    header->addSubview(title);

    auto hint = std::make_shared<PandaUI::Label>("M / Esc — close");
    hint->setFont(PandaUI::Font(11.f));
    hint->setTextColor(PandaUI::Color(0x6E7A8EFF));
    header->addSubview(hint);

    auto spacer = std::make_shared<PandaUI::Panel>();
    spacer->setBackgroundColor(PandaUI::Color(0x00000000));
    spacer->layout().setFlexGrow(1.f);
    header->addSubview(spacer);

    auto closeButton = std::make_shared<MenuButton>("Close");
    closeButton->layout().setWidth(PandaUI::Length::points(96.f));
    closeButton->layout().setHeight(PandaUI::Length::points(34.f));
    closeButton->setFont(PandaUI::Font(14.f));
    closeButton->setOnClick([](PandaUI::Button &) {
        GameMenu::setState(GameMenuState::Playing);
    });
    header->addSubview(closeButton);
    card->addSubview(header);

    // Тело: сайдбар разделов + контент активного редактора.
    auto body = std::make_shared<PandaUI::Panel>();
    body->setBackgroundColor(PandaUI::Color(0x00000000));
    body->layout().setFlexDirection(PandaUI::FlexDirection::Row);
    body->layout().setGap(16.f);
    body->layout().setWidth(PandaUI::Length::percent(100.f));
    body->layout().setFlexGrow(1.f);

    auto sidebar = std::make_shared<PandaUI::Panel>();
    sidebar->setBackgroundColor(PandaUI::Color(0x10141CB0));
    sidebar->surface().setCornerRadius(10.f);
    sidebar->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    sidebar->layout().setPadding(8.f);
    sidebar->layout().setWidth(PandaUI::Length::points(180.f));

    // Список разделов прокручивается, если окно ниже списка.
    auto sidebarScroll = std::make_shared<PandaUI::ScrollView>();
    sidebarScroll->layout().setFlexGrow(1.f);
    sidebarScroll->getContentView()->layout().setGap(3.f);
    sidebar->addSubview(sidebarScroll);

    auto content = std::make_shared<PandaUI::Panel>();
    content->setBackgroundColor(PandaUI::Color(0x00000000));
    content->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    content->layout().setGap(10.f);
    content->layout().setFlexGrow(1.f);

    m_sectionTitle = std::make_shared<PandaUI::Label>("");
    m_sectionTitle->setFont(PandaUI::Font(17.f));
    m_sectionTitle->setTextColor(PandaUI::Color(0xF4F7FFFF));
    content->addSubview(m_sectionTitle);

    // Общая прокрутка контента раздела; панели со встроенной прокруткой добавляются
    // мимо неё (прямо в колонку контента), чтобы не вкладывать скролл в скролл.
    m_scroll = std::make_shared<PandaUI::ScrollView>();
    m_scroll->layout().setFlexGrow(1.f);
    content->addSubview(m_scroll);

    const ArchUIContext context{blocks, std::move(previewFor), &m_blockCards, &m_elementCards};
    const auto addSection = [&](const char *name, std::shared_ptr<ArchPanel> panel) {
        const size_t index = m_sections.size();
        auto button = std::make_shared<SidebarButton>(name);
        button->setOnClick([this, index](PandaUI::Button &) { setSection(index); });
        ArchUI::setViewVisible(*panel, false);
        sidebarScroll->addContentSubview(button);
        if (panel->hasOwnScrolling()) {
            panel->layout().setFlexGrow(1.f);
            content->addSubview(panel);
        } else {
            m_scroll->addContentSubview(panel);
        }
        m_sections.push_back({std::move(panel), std::move(button)});
    };
    addSection("Walls", std::make_shared<WallsPanel>(context));
    addSection("Windows", std::make_shared<WindowsPanel>(context));
    addSection("Doors", std::make_shared<DoorsPanel>(context));
    addSection("Arches", std::make_shared<ArchesPanel>(context));
    addSection("Columns", std::make_shared<ColumnsPanel>(context));
    addSection("Pilasters", std::make_shared<PilastersPanel>(context));
    addSection("Cornices", std::make_shared<CornicesPanel>(context));
    addSection("Balconies", std::make_shared<BalconiesPanel>(context));
    addSection("Roofs", std::make_shared<RoofsPanel>(context));
    addSection("Domes", std::make_shared<DomesPanel>(context));
    addSection("Stairs", std::make_shared<StairsPanel>(context));
    addSection("Foundations", std::make_shared<FoundationsPanel>(context));
    addSection("Decor", std::make_shared<DecorPanel>(context));
    addSection("Materials", std::make_shared<MaterialsPanel>(context));
    addSection("Facades", std::make_shared<FacadesPanel>(context));
    addSection("Terrain", std::make_shared<TerrainPanel>(context));

    body->addSubview(sidebar);
    body->addSubview(content);
    card->addSubview(body);
    addSubview(card);
    setSection(0);
}

void ArchLibraryMenu::setSection(size_t index) {
    if (index >= m_sections.size()) { return; }
    m_activeSection = index;
    for (size_t i = 0; i < m_sections.size(); ++i) {
        ArchUI::setViewVisible(*m_sections[i].panel, i == index);
        std::static_pointer_cast<SidebarButton>(m_sections[i].button)->setActive(i == index);
    }
    // Раздел со своей прокруткой занимает колонку сам; общий скролл — только для остальных.
    ArchUI::setViewVisible(*m_scroll, !m_sections[index].panel->hasOwnScrolling());
    m_scroll->setContentOffset({0.f, 0.f});
    m_sectionTitle->setText(m_sections[index].button->getTitleLabel()->getText());
    m_sections[index].panel->refresh();
}

void ArchLibraryMenu::syncSelection(VoxelType block, ArchObjectType element) {
    for (auto &[type, card] : m_blockCards) {
        if (card) { card->setCardSelected(type == block); }
    }
    // Форма и материал независимы: подсветка формы не гасится выбором блока.
    for (auto &[type, card] : m_elementCards) {
        if (card) { card->setCardSelected(type == element); }
    }
    m_sections[m_activeSection].panel->refresh();
}
