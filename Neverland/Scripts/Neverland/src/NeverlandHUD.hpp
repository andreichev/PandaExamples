#pragma once

#include "BlocksCreation.hpp"
#include "GameMenu.hpp"
#include "Model/BlockPalette.hpp"
#include "NeverlandHUDLayout.hpp"
#include "TerrainBrushPanel.hpp"

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>
#include <PandaUI/PandaUI.hpp>

#include <array>
#include <memory>
#include <vector>

using namespace Bamboo;

class BlockCardButton;

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
    void buildUI();
    void updateTouchControlSafeArea();
    void updateSelection();
    void updateMenuInput();  // Esc: Playing ↔ Paused; M: Playing ↔ BlockPicker
    void updateBrushPanel(); // Alt-модификатор курсора + синк панели рельефа (desktop)
    void applyMenuState();   // видимость HUD/меню + идемпотентность по последнему состоянию
    void loadBlockPreviewTextures();
    void destroyBlockPreviewTextures();
    PandaUI::TextureHandle previewFor(VoxelType type) const;
    std::shared_ptr<PandaUI::Panel> makePauseMenu();
    // Меню выбора блоков (M): вкладки Building / Terrain.
    std::shared_ptr<PandaUI::Panel> makeBlocksMenu();
    std::shared_ptr<PandaUI::Panel> makeBuildingTab();
    std::shared_ptr<PandaUI::Panel> makeTerrainTab();
    void setBlocksMenuTab(int tab); // 0 — Building, 1 — Terrain
    std::shared_ptr<PandaUI::Panel> makeCrosshair();
    std::shared_ptr<PandaUI::Panel> makeTouchControls();
    std::shared_ptr<PandaUI::Panel> makeMoveStickOverlay();
    void updateMoveStickOverlay(); // позиция/видимость джойстика из NeverlandTouchControls
    std::shared_ptr<PandaUI::Panel> makeActionControls();
    std::shared_ptr<PandaUI::Panel> makeSelectionPill();
    std::shared_ptr<PandaUI::Panel> makeHotbar();
    std::shared_ptr<PandaUI::Button> makeSlot(int index);
    void applySlotStyle(size_t index, bool selected);
    void refreshHotbar(const std::vector<VoxelType> &recent); // MRU: превью слотов по списку

    PandaUI::Window m_window;
    Shared<BlocksCreation> m_blocksCreation;
    std::shared_ptr<PandaUI::Panel> m_root;
    std::shared_ptr<PandaUI::Panel> m_hudLayer;   // игровой HUD (прицел/хотбар/тач) — скрыт в меню
    std::shared_ptr<PandaUI::Panel> m_pauseMenu;  // пауза (Esc)
    std::shared_ptr<PandaUI::Panel> m_blocksMenu; // выбор блоков (M)
    std::shared_ptr<PandaUI::Panel> m_buildingTab; // вкладка строительства
    std::shared_ptr<PandaUI::Panel> m_terrainTab;  // вкладка рельефа
    std::array<std::shared_ptr<PandaUI::Button>, 2> m_tabButtons;
    // Задел под настройки выбранного элемента (параметры будущих сложных блоков).
    std::shared_ptr<PandaUI::Panel> m_elementSettings;
    int m_activeMenuTab = 0;
    GameMenuState m_appliedMenuState = GameMenuState::MainMenu;
    std::shared_ptr<PandaUI::Panel> m_stickBase; // подложка джойстика (видна при касании)
    std::shared_ptr<PandaUI::Panel> m_stickKnob; // ручка джойстика
    std::shared_ptr<TerrainBrushPanel> m_brushPanel; // инструмент рельефа (desktop)
    std::shared_ptr<PandaUI::Label> m_selectionLabel;
    std::array<std::shared_ptr<PandaUI::Button>, NeverlandHUDLayout::HotbarSlotCount> m_slots;
    // Контейнеры превью слотов (контент меняется при изменении MRU-списка).
    std::array<std::shared_ptr<PandaUI::Panel>, NeverlandHUDLayout::HotbarSlotCount> m_slotPreviews;
    std::vector<VoxelType> m_displayedHotbar; // отображённый MRU-список
    // Карточки меню: блоки по типу + элементы по типу (для подсветки выбора).
    std::vector<std::pair<VoxelType, std::shared_ptr<BlockCardButton>>> m_blockCards;
    std::vector<std::pair<ArchObjectType, std::shared_ptr<BlockCardButton>>> m_elementCards;
    // Превью тайлов по типу вокселя (строительные + природные).
    std::array<PandaUI::TextureHandle, static_cast<size_t>(VoxelType::COUNT)> m_previewTextures;
    VoxelType m_displayedSelection = VoxelType::NOTHING;
    bool m_displayedElementSelected = false;
};

REGISTER_SCRIPT(NeverlandHUD)
