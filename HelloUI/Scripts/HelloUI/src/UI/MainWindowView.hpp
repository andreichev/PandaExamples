#pragma once

#include <PandaUI/PandaUI.hpp>

#include <functional>
#include <memory>

namespace HelloUI {

class MainWindowView final : public PandaUI::Panel {
public:
    using Action = std::function<void()>;

    MainWindowView(Action onClick, Action onOpenWindow);

    void setClickCount(int clickCount);

    std::shared_ptr<PandaUI::ScrollView> getScrollView() const {
        return m_scrollView;
    }

private:
    std::shared_ptr<PandaUI::Label> m_statusLabel;
    std::shared_ptr<PandaUI::ScrollView> m_scrollView;
};

} // namespace HelloUI
