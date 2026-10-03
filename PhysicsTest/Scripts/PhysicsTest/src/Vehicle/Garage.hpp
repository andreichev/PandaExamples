#pragma once

#include <Bamboo/Base.hpp>

namespace Vehicle {

// The vehicles of the running world and the one the player drives. A vehicle script enters with
// its chassis in start() and leaves in shutdown(); the first one to enter gets the driver. The
// camera and the HUD follow the active vehicle; only it reads the input, the others stand on the
// brakes. ChaseCamera picks the vehicle to start with, and V passes the wheel to the next one.
namespace Garage {
    void enter(Bamboo::EntityHandle chassis, const char *name);
    void leave(Bamboo::EntityHandle chassis);
    bool isActive(Bamboo::EntityHandle chassis);
    Bamboo::EntityHandle active(); // invalid: the world has no vehicles
    const char *activeName();
    void activate(Bamboo::EntityHandle chassis); // a chassis that has not entered changes nothing
    void activateNext();
} // namespace Garage

} // namespace Vehicle
