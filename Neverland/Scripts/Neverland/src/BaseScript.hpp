#pragma once

#include "Model/ChunksStorage.hpp"

#include <Bamboo/Script.hpp>
#include <Bamboo/Bamboo.hpp>

using namespace Bamboo;

class BaseScript : public Script {
public:
    int var;
    MaterialHandle material;

    PANDA_FIELDS_BEGIN(BaseScript)
    PANDA_FIELD(var)
    PANDA_FIELD(material)
    PANDA_FIELDS_END

    void start() override;
    void update(float dt) override;
    void shutdown() override;
};

REGISTER_SCRIPT(BaseScript)
