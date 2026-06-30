#pragma once

#include "BlocksCreation.hpp"
#include "NeverlandHUDLayout.hpp"

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

    TextureHandle blocksTileTexture;

    PANDA_FIELDS_BEGIN(NeverlandHUD)
    PANDA_FIELD(blocksTileTexture)
    PANDA_FIELDS_END

private:
    struct BlockSlot {
        VoxelType type;
        const char *name;
    };

    void buildUI();
    void updateTouchControlSafeArea();
    void updateSelection();
    void loadBlockPreviewTextures();
    void destroyBlockPreviewTextures();
    std::shared_ptr<PandaUI::Panel> makeCrosshair();
    std::shared_ptr<PandaUI::Panel> makeTouchControls();
    std::shared_ptr<PandaUI::Panel> makeMovePad();
    std::shared_ptr<PandaUI::Panel> makeActionControls();
    std::shared_ptr<PandaUI::Panel> makeSelectionPill();
    std::shared_ptr<PandaUI::Panel> makeHotbar();
    std::shared_ptr<PandaUI::Button> makeSlot(const BlockSlot &slot, int index);
    void applySlotStyle(size_t index, bool selected);

    static constexpr std::array<BlockSlot, NeverlandHUDLayout::HotbarSlotCount> BLOCKS = {
        BlockSlot{VoxelType::GRASS, "Grass"},
        BlockSlot{VoxelType::GROUND, "Dirt"},
        BlockSlot{VoxelType::TREE, "Wood"},
        BlockSlot{VoxelType::BOARDS, "Boards"},
        BlockSlot{VoxelType::STONE, "Stone"},
        BlockSlot{VoxelType::STONE_BRICKS, "Bricks"},
        BlockSlot{VoxelType::SAND_STONE, "Sandstone"},
        BlockSlot{VoxelType::SAND, "Sand"},
        BlockSlot{VoxelType::BIRCH_LOG, "Birch"},
        BlockSlot{VoxelType::TALL_GRASS, "Tall Grass"},
    };

    PandaUI::Window m_window;
    Shared<BlocksCreation> m_blocksCreation;
    std::shared_ptr<PandaUI::Panel> m_root;
    std::shared_ptr<PandaUI::Label> m_selectionLabel;
    std::array<std::shared_ptr<PandaUI::Button>, BLOCKS.size()> m_slots;
    std::array<PandaUI::TextureHandle, BLOCKS.size()> m_previewTextures;
    VoxelType m_displayedSelection = VoxelType::NOTHING;
};

REGISTER_SCRIPT(NeverlandHUD)
