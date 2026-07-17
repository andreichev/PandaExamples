#include "HeldItemView.hpp"

#include "Model/GameContext.hpp"
#include "Model/TerrainMeshGenerator.hpp"
#include "Model/VoxelTextureMapper.hpp"

#include <Bamboo/Assets/AssetManagerAPI.hpp>
#include <Bamboo/Assets/MeshAPI.hpp>
#include <Bamboo/Components/MeshComponentAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/WorldAPI.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr float ATLAS_TILE_COUNT = 16.0f;
const Vec2 SOLID_WHITE_UV(0.969f, 0.031f);

Vec2 getAtlasUV(uint8_t tileIndex) {
    const float uvSize = 1.0f / ATLAS_TILE_COUNT;
    float u = static_cast<float>(tileIndex % 16) * uvSize;
    float v = static_cast<float>(tileIndex / 16) * uvSize;
    u += 0.0005f;
    v += 0.0005f;
    return {u, v};
}

Color toColor(uint32_t hex) {
    return Color(
        static_cast<float>((hex >> 24) & 0xFF) / 255.0f,
        static_cast<float>((hex >> 16) & 0xFF) / 255.0f,
        static_cast<float>((hex >> 8) & 0xFF) / 255.0f,
        static_cast<float>(hex & 0xFF) / 255.0f
    );
}

glm::vec3 toGlm(Vec3 value) {
    return glm::vec3(value.x, value.y, value.z);
}

Vec3 toBambooVec3(const glm::vec3 &value) {
    return Vec3(value.x, value.y, value.z);
}

glm::quat toGlmQuat(Quat value) {
    glm::quat result(value.w, value.x, value.y, value.z);
    if (glm::length(result) < 0.0001f) { return glm::quat(glm::vec3(0.0f)); }
    return glm::normalize(result);
}

Quat toBambooQuat(const glm::quat &value) {
    return Quat(value.x, value.y, value.z, value.w);
}

void addQuad(
    std::vector<Vertex> &vertices,
    std::vector<uint32_t> &indices,
    const Vec3 &p0,
    const Vec3 &p1,
    const Vec3 &p2,
    const Vec3 &p3,
    const Vec3 &normal,
    const Vec2 &uv,
    Color color,
    float light
) {
    constexpr float uvSize = 1.0f / ATLAS_TILE_COUNT - 0.001f;
    TerrainMeshGenerator::addFaceIndices(static_cast<uint32_t>(vertices.size()), indices);
    vertices.emplace_back(Vertex(p0, Vec2(uv.x, uv.y + uvSize), normal, color, light));
    vertices.emplace_back(Vertex(p1, Vec2(uv.x + uvSize, uv.y + uvSize), normal, color, light));
    vertices.emplace_back(Vertex(p2, Vec2(uv.x + uvSize, uv.y), normal, color, light));
    vertices.emplace_back(Vertex(p3, Vec2(uv.x, uv.y), normal, color, light));
}

void addSolidQuad(
    std::vector<Vertex> &vertices,
    std::vector<uint32_t> &indices,
    const Vec3 &p0,
    const Vec3 &p1,
    const Vec3 &p2,
    const Vec3 &p3,
    const Vec3 &normal,
    Color color,
    float light
) {
    TerrainMeshGenerator::addFaceIndices(static_cast<uint32_t>(vertices.size()), indices);
    vertices.emplace_back(Vertex(p0, SOLID_WHITE_UV, normal, color, light));
    vertices.emplace_back(Vertex(p1, SOLID_WHITE_UV, normal, color, light));
    vertices.emplace_back(Vertex(p2, SOLID_WHITE_UV, normal, color, light));
    vertices.emplace_back(Vertex(p3, SOLID_WHITE_UV, normal, color, light));
}

void addTexturedCuboid(
    std::vector<Vertex> &vertices,
    std::vector<uint32_t> &indices,
    const glm::vec3 &min,
    const glm::vec3 &max,
    const VoxelTextureData &texture
) {
    const Vec2 sideUV = getAtlasUV(texture.sideTileIndex);
    const Vec2 topUV = getAtlasUV(texture.topTileIndex);
    const Vec2 bottomUV = getAtlasUV(texture.bottomTileIndex);
    const Color sideColor = toColor(texture.sideColor);
    const Color topColor = toColor(texture.topColor);
    const Color bottomColor = toColor(texture.bottomColor);

    addQuad(
        vertices,
        indices,
        Vec3(min.x, min.y, max.z),
        Vec3(max.x, min.y, max.z),
        Vec3(max.x, max.y, max.z),
        Vec3(min.x, max.y, max.z),
        Vec3(0.0f, 0.0f, 1.0f),
        sideUV,
        sideColor,
        1.0f
    );
    addQuad(
        vertices,
        indices,
        Vec3(min.x, min.y, min.z),
        Vec3(min.x, max.y, min.z),
        Vec3(max.x, max.y, min.z),
        Vec3(max.x, min.y, min.z),
        Vec3(0.0f, 0.0f, -1.0f),
        sideUV,
        sideColor,
        0.75f
    );
    addQuad(
        vertices,
        indices,
        Vec3(min.x, max.y, min.z),
        Vec3(min.x, max.y, max.z),
        Vec3(max.x, max.y, max.z),
        Vec3(max.x, max.y, min.z),
        Vec3(0.0f, 1.0f, 0.0f),
        topUV,
        topColor,
        0.95f
    );
    addQuad(
        vertices,
        indices,
        Vec3(min.x, min.y, min.z),
        Vec3(max.x, min.y, min.z),
        Vec3(max.x, min.y, max.z),
        Vec3(min.x, min.y, max.z),
        Vec3(0.0f, -1.0f, 0.0f),
        bottomUV,
        bottomColor,
        0.85f
    );
    addQuad(
        vertices,
        indices,
        Vec3(min.x, min.y, min.z),
        Vec3(min.x, min.y, max.z),
        Vec3(min.x, max.y, max.z),
        Vec3(min.x, max.y, min.z),
        Vec3(-1.0f, 0.0f, 0.0f),
        sideUV,
        sideColor,
        0.9f
    );
    addQuad(
        vertices,
        indices,
        Vec3(max.x, min.y, min.z),
        Vec3(max.x, max.y, min.z),
        Vec3(max.x, max.y, max.z),
        Vec3(max.x, min.y, max.z),
        Vec3(1.0f, 0.0f, 0.0f),
        sideUV,
        sideColor,
        0.8f
    );
}

void addSolidCuboid(
    std::vector<Vertex> &vertices,
    std::vector<uint32_t> &indices,
    const glm::vec3 &min,
    const glm::vec3 &max,
    Color color
) {
    addSolidQuad(
        vertices,
        indices,
        Vec3(min.x, min.y, max.z),
        Vec3(max.x, min.y, max.z),
        Vec3(max.x, max.y, max.z),
        Vec3(min.x, max.y, max.z),
        Vec3(0.0f, 0.0f, 1.0f),
        color,
        0.98f
    );
    addSolidQuad(
        vertices,
        indices,
        Vec3(min.x, min.y, min.z),
        Vec3(min.x, max.y, min.z),
        Vec3(max.x, max.y, min.z),
        Vec3(max.x, min.y, min.z),
        Vec3(0.0f, 0.0f, -1.0f),
        color,
        0.9f
    );
    addSolidQuad(
        vertices,
        indices,
        Vec3(min.x, max.y, min.z),
        Vec3(min.x, max.y, max.z),
        Vec3(max.x, max.y, max.z),
        Vec3(max.x, max.y, min.z),
        Vec3(0.0f, 1.0f, 0.0f),
        color,
        1.0f
    );
    addSolidQuad(
        vertices,
        indices,
        Vec3(min.x, min.y, min.z),
        Vec3(max.x, min.y, min.z),
        Vec3(max.x, min.y, max.z),
        Vec3(min.x, min.y, max.z),
        Vec3(0.0f, -1.0f, 0.0f),
        color,
        0.82f
    );
    addSolidQuad(
        vertices,
        indices,
        Vec3(min.x, min.y, min.z),
        Vec3(min.x, min.y, max.z),
        Vec3(min.x, max.y, max.z),
        Vec3(min.x, max.y, min.z),
        Vec3(-1.0f, 0.0f, 0.0f),
        color,
        0.94f
    );
    addSolidQuad(
        vertices,
        indices,
        Vec3(max.x, min.y, min.z),
        Vec3(max.x, max.y, min.z),
        Vec3(max.x, max.y, max.z),
        Vec3(max.x, min.y, max.z),
        Vec3(1.0f, 0.0f, 0.0f),
        color,
        0.88f
    );
}

void addDoubleSidedPlantQuad(
    std::vector<Vertex> &vertices,
    std::vector<uint32_t> &indices,
    const Vec3 &p0,
    const Vec3 &p1,
    const Vec3 &p2,
    const Vec3 &p3,
    const Vec2 &uv,
    Color color
) {
    addQuad(vertices, indices, p0, p1, p2, p3, Vec3(0.0f, 1.0f, 0.0f), uv, color, 1.0f);
    addQuad(vertices, indices, p3, p2, p1, p0, Vec3(0.0f, -1.0f, 0.0f), uv, color, 1.0f);
}

MeshData makeHeldBlockMesh(VoxelType type) {
    Voxel voxel(type);
    VoxelTextureData &texture = VoxelTextureMapper::getTextureData(&voxel);
    MeshData data;
    if (type == VoxelType::TALL_GRASS) {
        const Vec2 uv = getAtlasUV(texture.sideTileIndex);
        const Color color = toColor(texture.sideColor);
        addDoubleSidedPlantQuad(
            data.vertices,
            data.indices,
            Vec3(-0.42f, -0.5f, -0.42f),
            Vec3(0.42f, -0.5f, 0.42f),
            Vec3(0.42f, 0.45f, 0.42f),
            Vec3(-0.42f, 0.45f, -0.42f),
            uv,
            color
        );
        addDoubleSidedPlantQuad(
            data.vertices,
            data.indices,
            Vec3(0.42f, -0.5f, -0.42f),
            Vec3(-0.42f, -0.5f, 0.42f),
            Vec3(-0.42f, 0.45f, 0.42f),
            Vec3(0.42f, 0.45f, -0.42f),
            uv,
            color
        );
        return data;
    }

    addTexturedCuboid(data.vertices, data.indices, glm::vec3(-0.5f), glm::vec3(0.5f), texture);
    return data;
}

MeshData makeHeldHandMesh() {
    MeshData data;
    const Color skin = toColor(0xE3A77DFF);
    addSolidCuboid(
        data.vertices,
        data.indices,
        glm::vec3(-0.08f, -0.08f, -0.55f),
        glm::vec3(0.08f, 0.08f, 0.08f),
        skin
    );
    addSolidCuboid(
        data.vertices,
        data.indices,
        glm::vec3(-0.12f, -0.11f, -0.12f),
        glm::vec3(0.12f, 0.11f, 0.14f),
        skin
    );
    return data;
}

} // namespace

void HeldItemView::update(
    EntityHandle cameraEntity, VoxelType selectedBlock, bool moving, bool sprinting, float deltaTime
) {
    if (selectedBlock == VoxelType::NOTHING || selectedBlock == VoxelType::COUNT) {
        shutdown();
        return;
    }

    ensureView();
    if (!m_blockEntity.isValid() || !m_handEntity.isValid()) { return; }

    if (m_displayedBlock != selectedBlock) { updateMesh(selectedBlock); }
    syncTransform(cameraEntity, moving, sprinting, deltaTime);
}

void HeldItemView::shutdown() {
    if (m_blockMesh.isValid()) {
        AssetManagerAPI::deleteMesh(m_blockMesh);
        m_blockMesh = {};
    }
    if (m_handMesh.isValid()) {
        AssetManagerAPI::deleteMesh(m_handMesh);
        m_handMesh = {};
    }
    if (m_blockEntity.isValid()) {
        WorldAPI::destroyEntity(m_blockEntity);
        m_blockEntity = {};
    }
    if (m_handEntity.isValid()) {
        WorldAPI::destroyEntity(m_handEntity);
        m_handEntity = {};
    }
    m_displayedBlock = VoxelType::NOTHING;
    m_appliedMaterialId = BAMBOO_INVALID_HANDLE;
}

void HeldItemView::ensureView() {
    if (!m_blockMesh.isValid()) { m_blockMesh = AssetManagerAPI::createMesh(); }
    if (!m_handMesh.isValid()) { m_handMesh = AssetManagerAPI::createMesh(); }

    if (!m_blockEntity.isValid() && m_blockMesh.isValid()) {
        m_blockEntity = WorldAPI::createEntity("Held Block");
        EntityAPI::addComponent(m_blockEntity, ComponentType::MESH_COMPONENT);
        MeshComponentAPI::setMesh(m_blockEntity, m_blockMesh);
    }
    if (!m_handEntity.isValid() && m_handMesh.isValid()) {
        m_handEntity = WorldAPI::createEntity("Held Hand");
        EntityAPI::addComponent(m_handEntity, ComponentType::MESH_COMPONENT);
        MeshComponentAPI::setMesh(m_handEntity, m_handMesh);
    }

    if (GameContext::s_chunkMaterial.isValid() &&
        m_appliedMaterialId != GameContext::s_chunkMaterial.id) {
        if (m_blockEntity.isValid()) {
            MeshComponentAPI::setMaterial(m_blockEntity, GameContext::s_chunkMaterial);
        }
        if (m_handEntity.isValid()) {
            MeshComponentAPI::setMaterial(m_handEntity, GameContext::s_chunkMaterial);
        }
        m_appliedMaterialId = GameContext::s_chunkMaterial.id;
    }
}

void HeldItemView::updateMesh(VoxelType selectedBlock) {
    if (m_blockMesh.isValid()) { MeshAPI::update(m_blockMesh, makeHeldBlockMesh(selectedBlock)); }
    if (m_handMesh.isValid()) { MeshAPI::update(m_handMesh, makeHeldHandMesh()); }
    m_displayedBlock = selectedBlock;
}

void HeldItemView::syncTransform(
    EntityHandle cameraEntity, bool moving, bool sprinting, float deltaTime
) {
    const float safeDeltaTime = std::max(deltaTime, 0.0f);
    const float targetMotion = moving ? 1.0f : 0.0f;
    const float targetSprint = sprinting ? 1.0f : 0.0f;
    const float motionBlendSpeed = moving ? 9.0f : 5.0f;
    m_motionBlend +=
        (targetMotion - m_motionBlend) * std::min(motionBlendSpeed * safeDeltaTime, 1.0f);

    const float blendSpeed = sprinting ? 8.0f : 6.0f;
    m_sprintBlend +=
        (targetSprint - m_sprintBlend) * std::min(blendSpeed * safeDeltaTime, 1.0f);

    const float motionSpeed = 0.65f + m_motionBlend * 1.45f + m_sprintBlend * 1.15f;
    const float motionAmount = m_motionBlend * (1.0f + m_sprintBlend * 0.45f);
    m_time += safeDeltaTime * motionSpeed;

    const glm::vec3 cameraPosition = toGlm(TransformComponentAPI::getPosition(cameraEntity));
    const glm::quat cameraRotation = toGlmQuat(TransformComponentAPI::getRotation(cameraEntity));
    const glm::vec3 front = glm::normalize(cameraRotation * glm::vec3(0.0f, 0.0f, -1.0f));
    const glm::vec3 right = glm::normalize(cameraRotation * glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::vec3 up = glm::normalize(cameraRotation * glm::vec3(0.0f, 1.0f, 0.0f));
    const float idleBob = std::sin(m_time * 1.45f) * 0.004f * (1.0f - m_motionBlend);
    const float bob = idleBob + std::sin(m_time * 2.8f) * 0.030f * motionAmount;
    const float sway = std::sin(m_time * 1.7f) * 0.060f * motionAmount;
    const float sprintPullback = m_sprintBlend * 0.075f;

    const glm::vec3 blockPosition = cameraPosition + front * (0.68f - sprintPullback + sway) +
                                    right * 0.34f + up * (-0.34f + bob);
    const glm::quat blockRotation =
        cameraRotation *
        glm::quat(glm::vec3(glm::radians(-18.0f), glm::radians(32.0f), glm::radians(10.0f)));
    TransformComponentAPI::setPosition(m_blockEntity, toBambooVec3(blockPosition));
    TransformComponentAPI::setRotation(m_blockEntity, toBambooQuat(blockRotation));
    TransformComponentAPI::setScale(m_blockEntity, Vec3(0.28f, 0.28f, 0.28f));

    const glm::vec3 handPosition = cameraPosition + front * (0.48f - sprintPullback * 0.7f) +
                                   right * 0.35f + up * (-0.48f + bob * 0.55f);
    const glm::quat handRotation =
        cameraRotation *
        glm::quat(glm::vec3(glm::radians(-28.0f), glm::radians(18.0f), glm::radians(8.0f)));
    TransformComponentAPI::setPosition(m_handEntity, toBambooVec3(handPosition));
    TransformComponentAPI::setRotation(m_handEntity, toBambooQuat(handRotation));
    TransformComponentAPI::setScale(m_handEntity, Vec3(0.82f, 0.82f, 0.82f));
}
