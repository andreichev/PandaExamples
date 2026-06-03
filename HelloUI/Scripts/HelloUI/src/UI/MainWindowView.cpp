#include "UI/MainWindowView.hpp"

#include "UI/DemoCardView.hpp"
#include "UI/HelloUIStyle.hpp"
#include "UI/MainHeaderView.hpp"
#include "UI/MainToolbarView.hpp"

#include <string>
#include <utility>

namespace HelloUI {

MainWindowView::MainWindowView(Action onClick, Action onOpenWindow) {
    setBackgroundColor(PandaUI::Color(0x0D111AFF));
    style().setFlexDirection(PandaUI::FlexDirection::Column);

    auto safeArea = std::make_shared<PandaUI::SafeAreaView>();
    safeArea->setBackgroundColor(transparent());
    safeArea->style().setFlexDirection(PandaUI::FlexDirection::Column);

    m_scrollView = std::make_shared<PandaUI::ScrollView>();
    m_scrollView->setBackgroundColor(PandaUI::Color(0x0D111AFF));
    m_scrollView->setScrollDirection(PandaUI::ScrollDirection::Vertical);
    m_scrollView->style().setFlexGrow(1.f);
    m_scrollView->style().setWidth(PandaUI::Length::percent(100.f));
    m_scrollView->getContentView()->style().setFlexDirection(PandaUI::FlexDirection::Column);
    m_scrollView->getContentView()->style().setPadding(24.f);
    m_scrollView->getContentView()->style().setGap(16.f);

    m_scrollView->addContentSubview(std::make_shared<MainHeaderView>());
    m_scrollView->addContentSubview(std::make_shared<MainToolbarView>(std::move(onClick), std::move(onOpenWindow)));

    m_statusLabel = makeLabel("Button clicks: 0", 16.f, PandaUI::Color(0xDCE4F2FF));
    m_scrollView->addContentSubview(m_statusLabel);

    m_scrollView->addContentSubview(std::make_shared<DemoCardView>(
        "Layout",
        "Flex direction, padding, gap, fixed height and grow are active here."
    ));
    m_scrollView->addContentSubview(std::make_shared<DemoCardView>(
        "Input",
        "Buttons use retained callbacks and keep state between frames."
    ));
    m_scrollView->addContentSubview(std::make_shared<DemoCardView>(
        "ScrollView",
        "The whole screen is inside one global PandaUI ScrollView."
    ));
    m_scrollView->addContentSubview(std::make_shared<DemoCardView>(
        "Runtime windows",
        "The Open window button creates a separate native PandaUI window."
    ));
    m_scrollView->addContentSubview(std::make_shared<DemoCardView>(
        "SDK path",
        "The game script owns PandaUI objects; runtime receives draw data and input events."
    ));
    m_scrollView->addContentSubview(makeFixedSpacer(64.f));

    safeArea->addSubview(m_scrollView);
    addSubview(safeArea);
}

void MainWindowView::setClickCount(int clickCount) {
    if (m_statusLabel) { m_statusLabel->setText("Button clicks: " + std::to_string(clickCount)); }
}

} // namespace HelloUI
