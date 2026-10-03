#include "Vehicle/VehicleHud.hpp"

#include "Vehicle/Garage.hpp"
#include "Vehicle/VehicleMath.hpp"

#include <Bamboo/Components/Rigidbody3DComponentAPI.hpp>
#include <Bamboo/Logger.hpp>

#include <string>

using namespace Vehicle;

namespace {

const char *CONTROLS = "W S  throttle, brake and reverse     A D  steer     Space  handbrake\n"
                       "R  back on the wheels     T  to the start     V  next vehicle";

std::shared_ptr<PandaUI::Label> makeLabel(const char *text, float size, PandaUI::Color color, int lines) {
    auto label = std::make_shared<PandaUI::Label>(text);
    label->setFont(PandaUI::Font(size));
    label->setTextColor(color);
    label->setNumberOfLines(lines);
    return label;
}

} // namespace

void VehicleHud::start() {
    m_window = PandaUI::Window::main();
    if (!m_window.isValid()) {
        LOG_ERROR("VehicleHud: the main PandaUI window is unavailable");
        return;
    }
    auto root = std::make_shared<PandaUI::Panel>();
    root->setBackgroundColor(PandaUI::Color(0x00000000));
    root->setUserInteractionEnabled(false);
    root->layout().setWidth(PandaUI::Length::percent(100.f));
    root->layout().setHeight(PandaUI::Length::percent(100.f));
    root->layout().setPadding(PandaUI::Edge::Horizontal, 24.f);
    root->layout().setPadding(PandaUI::Edge::Vertical, 20.f);
    root->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    root->layout().setGap(4.f);

    m_speedLabel = makeLabel("0 km/h", 34.f, PandaUI::Color(0xFFFFFFFF), 1);
    m_nameLabel = makeLabel("", 16.f, PandaUI::Color(0xFFD166FF), 1);
    root->addSubview(m_speedLabel);
    root->addSubview(m_nameLabel);
    root->addSubview(makeLabel(CONTROLS, 13.f, PandaUI::Color(0xFFFFFFB0), 2));
    m_window.setRootView(root);
}

void VehicleHud::update(float) {
    if (!m_speedLabel) { return; }
    EntityHandle vehicle = Garage::active();
    if (m_shownVehicle != vehicle.id) {
        m_shownVehicle = vehicle.id;
        m_nameLabel->setText(Garage::activeName());
    }
    int speed = 0;
    if (vehicle.isValid()) {
        const glm::vec3 velocity = toGlm(Rigidbody3DComponentAPI::getLinearVelocity(vehicle));
        speed = static_cast<int>(glm::length(velocity) * KMH_PER_MS + 0.5f);
    }
    if (speed != m_shownSpeed) {
        m_shownSpeed = speed;
        m_speedLabel->setText(std::to_string(speed) + " km/h");
    }
}

void VehicleHud::shutdown() {
    if (m_window.isValid()) { m_window.setRootView(nullptr); }
    m_speedLabel.reset();
    m_nameLabel.reset();
}
