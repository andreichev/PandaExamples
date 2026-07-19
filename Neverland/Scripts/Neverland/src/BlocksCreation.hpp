//
// Created by Admin on 12.02.2022.
//

#pragma once

#include "HeldItemView.hpp"
#include "Model/GameBrush.hpp"
#include "PlayerController.hpp"

#include <Bamboo/Bamboo.hpp>

class BlocksCreation final : public Script {
public:
    const float MAXIMUM_DISTANCE = 100.f;

    void start() override;
    void update(float deltaTime) override;
    void shutdown() override;
    VoxelType getSelectedBlock() const {
        return m_selectedBlock;
    }
    // Материал (блок). Форму (element) не трогает: блок — материал, элемент — форма.
    void setSelectedBlock(VoxelType type);
    // Форма постройки (Basic/Beam/Wall). Материал — текущий строительный блок.
    void setSelectedElement(ArchObjectType element);
    ArchObjectType getSelectedElement() const {
        return m_selectedElement;
    }
    bool isElementSelected() const {
        return m_selectedElement != ArchObjectType::Block;
    }
    // Недавние строительные материалы (MRU, front = текущий) — источник хотбара.
    const std::vector<VoxelType> &getRecentBlocks() const {
        return m_recentBlocks;
    }
    // Пресеты элементов (редакторы в меню): применяются к новым установкам.
    uint8_t getElementParam(ArchObjectType element, int index) const;
    void setElementParam(ArchObjectType element, int index, uint8_t value);
    // Кисть рельефа (панель TerrainBrushPanel и клавиши делят один стейт).
    void setBrushMode(GameBrushMode mode) { m_brushMode = mode; }
    GameBrushMode getBrushMode() const { return m_brushMode; }
    void setBrushSize(int size);
    int getBrushSize() const { return m_brushSize; }
    static int brushSizeCount();

private:
    struct AimHit {
        bool valid = false;
        int x = 0, y = 0, z = 0;              // воксель попадания (мировой)
        int normalX = 0, normalY = 0, normalZ = 0; // грань
        bool isBuilding = false;              // попали в куб построек (иначе рельеф)
    };

    Vec3 getPosition();
    AimHit aim(const Vec3 &origin, const Vec3 &direction); // ближайший из рельефа и кубов
    void updateSelectedBlock();
    void updateBrushSize();           // клавиши [ ] — размер кисти, B — режим
    float currentBrushRadius() const; // 0 = одиночный воксель
    void updateBrushMarker(const std::vector<TerrainAccess::Edit> &edits); // подсветка вокселей
    void destroyBrushMarker();
    void updateTouchBlockInput(
        float deltaTime, bool &placePressed, bool &breakPressed, Vec2 &touchAim, bool &hasTouchAim
    );

    struct TouchActionState {
        int id = -1;
        float startX = 0.0f;
        float startY = 0.0f;
        float lastX = 0.0f;
        float lastY = 0.0f;
        float duration = 0.0f;
        float repeatTimer = 0.0f;
        bool active = false;
        bool moved = false;
    };

    // Объект под установку по прицелу: origin за гранью, поворот от взгляда, материал.
    ArchitectureObject pendingObject(const AimHit &hit) const;
    // Протяжка (desktop): линия объектов от старта до прицела вдоль доминантной оси.
    void rebuildDragLine(const ArchitectureObject &current);

    Shared<PlayerController> m_playerController;
    HeldItemView m_heldItemView;
    TouchActionState m_touchAction;
    VoxelType m_selectedBlock;
    ArchObjectType m_selectedElement = ArchObjectType::Block;
    VoxelType m_lastBuildingMaterial = VoxelType::STONE_BRICKS;
    std::vector<VoxelType> m_recentBlocks; // MRU строительных материалов (хотбар)
    uint8_t m_roofParams[ARCH_PARAM_COUNT] = {};   // пресеты крыши (редактор в меню)
    uint8_t m_windowParams[ARCH_PARAM_COUNT] = {}; // пресеты окна
    bool m_dragActive = false;             // протяжка ЛКМ (desktop, building-режим)
    ArchitectureObject m_dragBase;         // первый объект линии
    std::vector<ArchitectureObject> m_dragLine;
    int m_brushSize = 2;
    GameBrushMode m_brushMode = GameBrushMode::Sphere;
    std::vector<TerrainAccess::Edit> m_previewEdits; // буфер превью/применения
    EntityHandle m_markerEntity;                     // визуальный маркер кисти (только desktop)
    MeshHandle m_markerMesh;
    uint64_t m_markerSignature = 0; // сигнатура набора подсвеченных вокселей
    bool m_markerVisible = false;
};

REGISTER_SCRIPT(BlocksCreation)
