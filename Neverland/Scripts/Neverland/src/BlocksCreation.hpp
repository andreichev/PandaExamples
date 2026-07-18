//
// Created by Admin on 12.02.2022.
//

#pragma once

#include "HeldItemView.hpp"
#include "Model/ChunksStorage.hpp"
#include "Model/TerrainBrush.hpp"
#include "PlayerController.hpp"

#include <Bamboo/Bamboo.hpp>

class BlocksCreation final : public Script {
public:
    const int MAXIMUM_DISTANCE = 100;

    void start() override;
    void update(float deltaTime) override;
    void shutdown() override;
    VoxelType getSelectedBlock() const {
        return m_selectedBlock;
    }
    void setSelectedBlock(VoxelType type);
    // Кисть рельефа (панель TerrainBrushPanel и клавиши делят один стейт).
    void setBrushMode(TerrainBrushMode mode) { m_brushMode = mode; }
    TerrainBrushMode getBrushMode() const { return m_brushMode; }
    void setBrushSize(int size);
    int getBrushSize() const { return m_brushSize; }
    static int brushSizeCount();

private:
    Vec3 getPosition();
    void setVoxel(int x, int y, int z, VoxelType type);
    void applyEdits(const std::vector<TerrainBrushEdit> &edits); // батч правок + ремеш чанков
    void updateChunk(const ChunkCoord &coord);
    void updateSelectedBlock();
    void updateBrushSize();                                       // клавиши [ ] — размер кисти терраформа
    float currentBrushRadius() const;                             // 0 = одиночный воксель
    void updateBrushMarker(const std::vector<TerrainBrushEdit> &edits); // подсветка вокселей (desktop)
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

    Shared<PlayerController> m_playerController;
    HeldItemView m_heldItemView;
    TouchActionState m_touchAction;
    VoxelType m_selectedBlock;
    int m_brushSize = 2;              // индекс в BRUSH_RADII (0 — одиночный воксель)
    TerrainBrushMode m_brushMode = TerrainBrushMode::Sphere;
    std::vector<TerrainBrushEdit> m_previewEdits; // буфер превью/применения (без реаллокаций)
    EntityHandle m_markerEntity;      // визуальный маркер кисти (только desktop)
    MeshHandle m_markerMesh;
    uint64_t m_markerSignature = 0;   // сигнатура набора подсвеченных вокселей
    bool m_markerVisible = false;
};

REGISTER_SCRIPT(BlocksCreation)
