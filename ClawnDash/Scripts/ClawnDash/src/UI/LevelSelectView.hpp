#pragma once

#include "Model/DashTypes.hpp"

#include <PandaUI/PandaUI.hpp>

#include <cstddef>
#include <functional>
#include <vector>

namespace ClawnDash {

class LevelSelectView final : public PandaUI::Panel {
public:
    using LevelAction = std::function<void(std::size_t)>;
    using Action = std::function<void()>;

    LevelSelectView(const std::vector<LevelInfo> &levels, LevelAction playLevel, Action back);
};

} // namespace ClawnDash
