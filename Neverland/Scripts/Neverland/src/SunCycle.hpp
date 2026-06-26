#pragma once

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>

using namespace Bamboo;

class SunCycle final : public Script {
public:
    int enabled = 1;
    float cycleSeconds = 180.0f;
    float minSunIntensity = 0.02f;
    float maxSunIntensity = 0.85f;
    float minAmbientIntensity = 0.06f;
    float maxAmbientIntensity = 0.36f;

    PANDA_FIELDS_BEGIN(SunCycle)
    PANDA_FIELD(enabled)
    PANDA_FIELD(cycleSeconds)
    PANDA_FIELD(minSunIntensity)
    PANDA_FIELD(maxSunIntensity)
    PANDA_FIELD(minAmbientIntensity)
    PANDA_FIELD(maxAmbientIntensity)
    PANDA_FIELDS_END

    void start() override;
    void update(float deltaTime) override;

private:
    void syncLighting();
};

REGISTER_SCRIPT(SunCycle)
