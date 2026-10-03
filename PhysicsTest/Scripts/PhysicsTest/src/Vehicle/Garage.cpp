#include "Vehicle/Garage.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace Vehicle {

namespace {

    struct Slot {
        Bamboo::EntityHandle chassis;
        std::string name;
    };

    // The script bundle outlives a Play session, so the list is emptied by the vehicles themselves
    // (leave in shutdown), not by static destruction.
    std::vector<Slot> s_slots;
    size_t s_active = 0;

} // namespace

namespace Garage {

    void enter(Bamboo::EntityHandle chassis, const char *name) {
        leave(chassis);
        s_slots.push_back({chassis, name});
    }

    void leave(Bamboo::EntityHandle chassis) {
        for (size_t i = 0; i < s_slots.size(); i++) {
            if (s_slots[i].chassis.id != chassis.id) { continue; }
            s_slots.erase(s_slots.begin() + static_cast<std::ptrdiff_t>(i));
            if (s_active > i) { s_active--; }
            break;
        }
        if (s_active >= s_slots.size()) { s_active = 0; }
    }

    bool isActive(Bamboo::EntityHandle chassis) {
        return !s_slots.empty() && s_slots[s_active].chassis.id == chassis.id;
    }

    Bamboo::EntityHandle active() {
        return s_slots.empty() ? Bamboo::EntityHandle() : s_slots[s_active].chassis;
    }

    const char *activeName() {
        return s_slots.empty() ? "" : s_slots[s_active].name.c_str();
    }

    void activate(Bamboo::EntityHandle chassis) {
        for (size_t i = 0; i < s_slots.size(); i++) {
            if (s_slots[i].chassis.id == chassis.id) { s_active = i; }
        }
    }

    void activateNext() {
        if (s_slots.empty()) { return; }
        s_active = (s_active + 1) % s_slots.size();
    }

} // namespace Garage

} // namespace Vehicle
