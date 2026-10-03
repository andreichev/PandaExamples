#pragma once

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>
#include <PandaUI/PandaUI.hpp>

#include <memory>

using namespace Bamboo;

// The driver's display: the name and the speed of the vehicle the player drives (Vehicle::Garage)
// and the controls. One per world, on any entity.
class VehicleHud : public Script {
public:
    void start() override;
    void update(float deltaTime) override;
    void shutdown() override;

private:
    PandaUI::Window m_window;
    std::shared_ptr<PandaUI::Label> m_speedLabel;
    std::shared_ptr<PandaUI::Label> m_nameLabel;
    int m_shownSpeed = -1;
    uint32_t m_shownVehicle = 0;
};

REGISTER_SCRIPT(VehicleHud, 100)
