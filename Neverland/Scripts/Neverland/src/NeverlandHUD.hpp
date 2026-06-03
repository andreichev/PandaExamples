#pragma once

#include "BlocksCreation.hpp"

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>
#include <PandaUI/PandaUI.hpp>

#include <array>
#include <memory>

using namespace Bamboo;

class NeverlandHUD final : public Script {
public:
    void start() override;
    void update(float deltaTime) override;
    void shutdown() override;

private:
    struct BlockSlot {
        VoxelType type;
        const char *name;
        uint32_t color;
    };

    void buildUI();
    void updateSelection();
    std::shared_ptr<PandaUI::Panel> makeCrosshair();
    std::shared_ptr<PandaUI::Panel> makeHotbar();
    std::shared_ptr<PandaUI::Panel> makeSlot(const BlockSlot &slot, int index);
    void applySlotStyle(size_t index, bool selected);

    static constexpr std::array<BlockSlot, 8> BLOCKS = {
        BlockSlot{VoxelType::GRASS, "Grass", 0x79C85AFF},
        BlockSlot{VoxelType::GROUND, "Dirt", 0x8C5A32FF},
        BlockSlot{VoxelType::TREE, "Wood", 0x8A5A2BFF},
        BlockSlot{VoxelType::BOARDS, "Boards", 0xB9894EFF},
        BlockSlot{VoxelType::STONE, "Stone", 0x8E9092FF},
        BlockSlot{VoxelType::STONE_BRICKS, "Bricks", 0x70777DFF},
        BlockSlot{VoxelType::SAND_STONE, "Sandstone", 0xD7B56CFF},
        BlockSlot{VoxelType::SAND, "Sand", 0xE2D58BFF},
    };

    PandaUI::Window m_window;
    Shared<BlocksCreation> m_blocksCreation;
    std::shared_ptr<PandaUI::Panel> m_root;
    std::array<std::shared_ptr<PandaUI::Panel>, BLOCKS.size()> m_slots;
    VoxelType m_displayedSelection = VoxelType::NOTHING;
};

REGISTER_SCRIPT(NeverlandHUD)
