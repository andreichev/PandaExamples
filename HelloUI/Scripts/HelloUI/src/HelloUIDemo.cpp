#include "HelloUIDemo.hpp"

#include <Bamboo/Logger.hpp>

#include <string>

namespace {

PandaUI::Color transparent() {
    return PandaUI::Color(0x00000000);
}

std::shared_ptr<PandaUI::Label> makeLabel(const char *text, float size, PandaUI::Color color) {
    auto label = std::make_shared<PandaUI::Label>(text);
    label->setFontSize(size);
    label->setTextColor(color);
    label->style().setWidth(PandaUI::Length::percent(100.f));
    return label;
}

std::shared_ptr<PandaUI::Spacer> makeFixedSpacer(float height) {
    auto spacer = std::make_shared<PandaUI::Spacer>();
    spacer->style().setFlexGrow(0.f);
    spacer->style().setHeight(PandaUI::Length::points(height));
    return spacer;
}

std::shared_ptr<PandaUI::Panel> makeContentRoot(float padding) {
    auto content = std::make_shared<PandaUI::Panel>();
    content->setBackgroundColor(transparent());
    content->style().setWidth(PandaUI::Length::percent(100.f));
    content->style().setFlexGrow(1.f);
    content->style().setFlexDirection(PandaUI::FlexDirection::Column);
    content->style().setPadding(padding);
    return content;
}

} // namespace

void HelloUIDemo::start() {
    buildMainWindow();
    LOG_INFO("HelloUI: PandaUI demo started");
}

void HelloUIDemo::shutdown() {
    for (PandaUI::Window &window : m_demoWindows) {
        if (window.isValid()) { window.close(); }
    }
    m_demoWindows.clear();
    if (m_mainWindow.isValid()) { m_mainWindow.setRootView(nullptr); }
    m_statusLabel.reset();
}

std::shared_ptr<PandaUI::Panel> HelloUIDemo::makeCard(const char *title, const char *text) {
    auto card = std::make_shared<PandaUI::Panel>();
    card->setBackgroundColor(PandaUI::Color(0x202637FF));
    card->setClipsToBounds(true);
    card->style().setWidth(PandaUI::Length::percent(100.f));
    card->style().setFlexDirection(PandaUI::FlexDirection::Column);
    card->style().setPadding(16.f);
    card->style().setGap(6.f);
    card->addSubview(makeLabel(title, 18.f, PandaUI::Color(0xF7FAFFFF)));
    card->addSubview(makeLabel(text, 13.f, PandaUI::Color(0xAEB9CCFF)));
    return card;
}

std::shared_ptr<PandaUI::Button> HelloUIDemo::makeButton(const char *title, PandaUI::Color color) {
    auto button = std::make_shared<PandaUI::Button>(title);
    button->style().setHeight(PandaUI::Length::points(42.f));
    button->style().setWidth(PandaUI::Length::points(168.f));
    button->setNormalColor(color);
    button->setHoveredColor(PandaUI::Color(0x4A92FFFF));
    button->setPressedColor(PandaUI::Color(0x1C58BDFF));
    button->setTextColor(PandaUI::Color(0xFFFFFFFF));
    return button;
}

void HelloUIDemo::buildMainWindow() {
    m_mainWindow = PandaUI::Window::main();
    if (!m_mainWindow.isValid()) {
        LOG_ERROR("HelloUI: main PandaUI window is unavailable");
        return;
    }

    auto root = std::make_shared<PandaUI::Panel>();
    root->setBackgroundColor(PandaUI::Color(0x0D111AFF));
    root->style().setFlexDirection(PandaUI::FlexDirection::Column);

    auto safeArea = std::make_shared<PandaUI::SafeAreaView>();
    safeArea->setBackgroundColor(transparent());
    safeArea->style().setFlexDirection(PandaUI::FlexDirection::Column);

    auto content = makeContentRoot(24.f);
    content->style().setGap(16.f);

    auto header = std::make_shared<PandaUI::Panel>();
    header->setBackgroundColor(PandaUI::Color(0x151B27FF));
    header->style().setHeight(PandaUI::Length::points(116.f));
    header->style().setFlexDirection(PandaUI::FlexDirection::Column);
    header->style().setPadding(18.f);
    header->style().setGap(8.f);
    header->addSubview(makeLabel("HelloUI", 28.f, PandaUI::Color(0xFFFFFFFF)));
    header->addSubview(makeLabel("Test project for retained-mode PandaUI from game scripting.", 15.f, PandaUI::Color(0x95A3BAFF)));
    content->addSubview(header);

    auto toolbar = std::make_shared<PandaUI::Panel>();
    toolbar->setBackgroundColor(transparent());
    toolbar->style().setHeight(PandaUI::Length::points(48.f));
    toolbar->style().setFlexDirection(PandaUI::FlexDirection::Row);
    toolbar->style().setGap(12.f);

    auto clickButton = makeButton("Click me", PandaUI::Color(0x2E7DFFFF));
    clickButton->setOnClick([this](PandaUI::Button &) {
        ++m_clickCount;
        if (m_statusLabel) { m_statusLabel->setText("Button clicks: " + std::to_string(m_clickCount)); }
    });

    auto windowButton = makeButton("Open window", PandaUI::Color(0x18A999FF));
    windowButton->setOnClick([this](PandaUI::Button &) {
        openDemoWindow();
    });

    toolbar->addSubview(clickButton);
    toolbar->addSubview(windowButton);
    content->addSubview(toolbar);

    m_statusLabel = makeLabel("Button clicks: 0", 16.f, PandaUI::Color(0xDCE4F2FF));
    content->addSubview(m_statusLabel);

    auto scroll = std::make_shared<PandaUI::ScrollView>();
    scroll->style().setFlexGrow(1.f);
    scroll->setBackgroundColor(PandaUI::Color(0x111722FF));
    scroll->getContentView()->style().setFlexDirection(PandaUI::FlexDirection::Column);
    scroll->getContentView()->style().setGap(12.f);
    scroll->addContentSubview(makeCard("Layout", "Flex direction, padding, gap, fixed height and grow are active here."));
    scroll->addContentSubview(makeCard("Input", "Buttons use retained callbacks and keep state between frames."));
    scroll->addContentSubview(makeCard("ScrollView", "This area is scrollable and uses PandaUI bounce/overscroll behavior."));
    scroll->addContentSubview(makeCard("Runtime windows", "The Open window button creates a separate native PandaUI window."));
    scroll->addContentSubview(makeCard("SDK path", "The game script owns PandaUI objects; runtime receives draw data and input events."));
    scroll->addContentSubview(makeFixedSpacer(64.f));
    content->addSubview(scroll);

    safeArea->addSubview(content);
    root->addSubview(safeArea);
    m_mainWindow.setRootView(root);
}

void HelloUIDemo::openDemoWindow() {
    PandaUI::Window window = PandaUI::Window::create({
        "HelloUI Demo Window",
        120.f,
        120.f,
        620.f,
        420.f,
    });
    if (!window.isValid()) {
        LOG_ERROR("HelloUI: failed to create PandaUI demo window");
        return;
    }

    auto root = std::make_shared<PandaUI::Panel>();
    root->setBackgroundColor(PandaUI::Color(0x111318FF));
    root->style().setFlexDirection(PandaUI::FlexDirection::Column);

    auto safeArea = std::make_shared<PandaUI::SafeAreaView>();
    safeArea->setBackgroundColor(transparent());
    safeArea->style().setFlexDirection(PandaUI::FlexDirection::Column);

    auto content = makeContentRoot(22.f);
    content->style().setGap(14.f);
    content->addSubview(makeLabel("Secondary PandaUI Window", 24.f, PandaUI::Color(0xFFFFFFFF)));
    content->addSubview(makeLabel("This window is created from a script and rendered by Panda runtime.", 14.f, PandaUI::Color(0xAEB9CCFF)));

    auto row = std::make_shared<PandaUI::Panel>();
    row->setBackgroundColor(transparent());
    row->style().setHeight(PandaUI::Length::points(120.f));
    row->style().setFlexDirection(PandaUI::FlexDirection::Row);
    row->style().setGap(12.f);
    row->addSubview(makeCard("Panel", "Stable View object"));
    row->addSubview(makeCard("Label", "Intrinsic text size"));
    row->addSubview(makeCard("Button", "Pointer events"));
    for (const auto &child : row->getSubviews()) {
        child->style().setFlexGrow(1.f);
    }
    content->addSubview(row);

    auto closeButton = makeButton("Close", PandaUI::Color(0xE85D75FF));
    closeButton->setOnClick([window](PandaUI::Button &) mutable {
        window.close();
    });
    content->addSubview(closeButton);

    safeArea->addSubview(content);
    root->addSubview(safeArea);
    window.setRootView(root);
    m_demoWindows.push_back(window);
}
