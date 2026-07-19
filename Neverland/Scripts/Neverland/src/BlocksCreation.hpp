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
    void setSelectedBlock(VoxelType type); // сбрасывает выбранный элемент (обычный блок)
    // Многоклеточный элемент (Beam): материал — текущий строительный блок.
    void setSelectedElement(ArchObjectType element);
    ArchObjectType getSelectedElement() const {
        return m_selectedElement;
    }
    bool isElementSelected() const {
        return m_selectedElement != ArchObjectType::Block;
    }
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

    Shared<PlayerController> m_playerController;
    HeldItemView m_heldItemView;
    TouchActionState m_touchAction;
    VoxelType m_selectedBlock;
    ArchObjectType m_selectedElement = ArchObjectType::Block;
    int m_brushSize = 2;
    GameBrushMode m_brushMode = GameBrushMode::Sphere;
    std::vector<TerrainAccess::Edit> m_previewEdits; // буфер превью/применения
    EntityHandle m_markerEntity;                     // визуальный маркер кисти (только desktop)
    MeshHandle m_markerMesh;
    uint64_t m_markerSignature = 0; // сигнатура набора подсвеченных вокселей
    bool m_markerVisible = false;
};

REGISTER_SCRIPT(BlocksCreation)
