#pragma once

#include "BlocksCreation.hpp"
#include "GameMenu.hpp"
#include "NeverlandHUDLayout.hpp"
#include "TerrainBrushPanel.hpp"

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

    TextureHandle blocksTileTexture; // base_materials_1 (7×7) — рукотворные блоки
    TextureHandle groundTileTexture; // base_ground_1 (6×6) — природные (терраформинг)

    PANDA_FIELDS_BEGIN(NeverlandHUD)
    PANDA_FIELD(blocksTileTexture)
    PANDA_FIELD(groundTileTexture)
    PANDA_FIELDS_END

private:
    struct BlockSlot {
        VoxelType type;
        const char *name;
    };

    void buildUI();
    void updateTouchControlSafeArea();
    void updateSelection();
    void updateMenuInput();  // Esc: Playing ↔ Paused
    void updateBrushPanel(); // Alt-модификатор курсора + синк панели кисти (desktop)
    void applyMenuState();   // видимость HUD/меню + идемпотентность по последнему состоянию
    void loadBlockPreviewTextures();
    void destroyBlockPreviewTextures();
    std::shared_ptr<PandaUI::Panel> makeMainMenu();
    std::shared_ptr<PandaUI::Panel> makePauseMenu();
    std::shared_ptr<PandaUI::Panel> makeCrosshair();
    std::shared_ptr<PandaUI::Panel> makeTouchControls();
    std::shared_ptr<PandaUI::Panel> makeMoveStickOverlay();
    void updateMoveStickOverlay(); // позиция/видимость джойстика из NeverlandTouchControls
    std::shared_ptr<PandaUI::Panel> makeActionControls();
    std::shared_ptr<PandaUI::Panel> makeSelectionPill();
    std::shared_ptr<PandaUI::Panel> makeHotbar();
    std::shared_ptr<PandaUI::Button> makeSlot(const BlockSlot &slot, int index);
    void applySlotStyle(size_t index, bool selected);

    // Терраформинг (природные, гладкий рельеф) + стройка (кубы из materials-атласа).
    static constexpr std::array<BlockSlot, NeverlandHUDLayout::HotbarSlotCount> BLOCKS = {
        BlockSlot{VoxelType::GRASS, "Grass"},
        BlockSlot{VoxelType::GROUND, "Dirt"},
        BlockSlot{VoxelType::STONE, "Stone"},
        BlockSlot{VoxelType::SAND, "Sand"},
        BlockSlot{VoxelType::BOARDS, "Plaster"},
        BlockSlot{VoxelType::STONE_BRICKS, "Bricks"},
        BlockSlot{VoxelType::SAND_STONE, "Stone Block"},
    };

    PandaUI::Window m_window;
    Shared<BlocksCreation> m_blocksCreation;
    std::shared_ptr<PandaUI::Panel> m_root;
    std::shared_ptr<PandaUI::Panel> m_hudLayer;  // игровой HUD (прицел/хотбар/тач) — скрыт в меню
    std::shared_ptr<PandaUI::Panel> m_mainMenu;  // стартовый экран
    std::shared_ptr<PandaUI::Panel> m_pauseMenu; // пауза (Esc)
    GameMenuState m_appliedMenuState = GameMenuState::MainMenu;
    std::shared_ptr<PandaUI::Panel> m_stickBase; // подложка джойстика (видна при касании)
    std::shared_ptr<PandaUI::Panel> m_stickKnob; // ручка джойстика
    std::shared_ptr<TerrainBrushPanel> m_brushPanel; // панель кисти terrain (desktop)
    std::shared_ptr<PandaUI::Label> m_selectionLabel;
    std::array<std::shared_ptr<PandaUI::Button>, BLOCKS.size()> m_slots;
    std::array<PandaUI::TextureHandle, BLOCKS.size()> m_previewTextures;
    VoxelType m_displayedSelection = VoxelType::NOTHING;
};

REGISTER_SCRIPT(NeverlandHUD)
