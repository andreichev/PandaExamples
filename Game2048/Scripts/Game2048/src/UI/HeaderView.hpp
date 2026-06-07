#pragma once

#include <PandaUI/PandaUI.hpp>

#include <functional>
#include <memory>

namespace Game2048 {

class HeaderView final : public PandaUI::Panel {
public:
    using Action = std::function<void()>;

    explicit HeaderView(Action onRestart);

    void setScore(int score);
    void setContentWidth(float width);

private:
    std::shared_ptr<PandaUI::Label> m_scoreLabel;
};

} // namespace Game2048
