#include "BlockModel.hpp"

namespace {
#include "BlockModelsGenerated.inl"
}

namespace BlockModels {

const BlockModelData *all(size_t &outCount) {
    outCount = BLOCK_MODEL_COUNT;
    return BLOCK_MODELS;
}

const BlockModelData *byId(uint8_t id) {
    for (size_t i = 0; i < BLOCK_MODEL_COUNT; i++) {
        if (BLOCK_MODELS[i].id == id) { return &BLOCK_MODELS[i]; }
    }
    return nullptr;
}

} // namespace BlockModels
