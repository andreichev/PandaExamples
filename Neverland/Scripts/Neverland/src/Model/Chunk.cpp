//
// Created by Admin on 12.02.2022.
//

#include "Chunk.hpp"

#include <Bamboo/Assets/AssetManagerAPI.hpp>
#include <Bamboo/Assets/MeshAPI.hpp>
#include <Bamboo/WorldAPI.hpp>
#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/Components/MeshComponentAPI.hpp>

int ChunkData::index(int x, int y, int z) {
    return y * SIZE_X * SIZE_Z + x * SIZE_Z + z;
}

std::size_t ChunkCoordHash::operator()(const ChunkCoord &coord) const noexcept {
    std::size_t seed = 0;
    auto combine = [&seed](int value) {
        std::size_t hash = static_cast<std::size_t>(value);
        seed ^= hash + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    };
    combine(coord.x);
    combine(coord.y);
    combine(coord.z);
    return seed;
}

Chunk::~Chunk() {
    destroyView();
}

Chunk::Chunk() = default;

namespace {

void destroyLayer(ChunkLayerView &layer) {
    if (layer.mesh.isValid()) {
        AssetManagerAPI::deleteMesh(layer.mesh);
        layer.mesh = {};
    }
    if (layer.entity.isValid()) {
        WorldAPI::destroyEntity(layer.entity);
        layer.entity = {};
    }
}

void updateLayer(ChunkLayerView &layer, const MeshData &meshData, const char *entityName) {
    if (meshData.vertices.empty() || meshData.indices.empty()) {
        destroyLayer(layer);
        return;
    }
    if (!layer.mesh.isValid()) {
        layer.mesh = AssetManagerAPI::createMesh();
        layer.entity = WorldAPI::createEntity(entityName);
        EntityAPI::addComponent(layer.entity, ComponentType::MESH_COMPONENT);
        MeshComponentAPI::setMesh(layer.entity, layer.mesh);
    }
    MeshAPI::update(layer.mesh, meshData);
}

} // namespace

void Chunk::destroyView() {
    destroyLayer(m_view.terrain);
    destroyLayer(m_view.blocks);
    m_data.meshUploaded = false;
    m_data.vertexCount = 0;
    m_data.indexCount = 0;
}

bool Chunk::set(int x, int y, int z, VoxelType type) {
    if (x < 0 || y < 0 || z < 0 || x >= SIZE_X || y >= SIZE_Y || z >= SIZE_Z) return false;
    Voxel &voxel = m_data.voxels[ChunkData::index(x, y, z)];
    if (voxel.type == type) { return false; }
    voxel.type = type;
    m_data.version++;
    m_data.needsRemesh = true;
    m_data.modifiedByPlayer = true;
    return true;
}

Voxel *Chunk::get(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= SIZE_X || y >= SIZE_Y || z >= SIZE_Z) return nullptr;
    return &m_data.voxels[ChunkData::index(x, y, z)];
}

const Voxel *Chunk::get(int x, int y, int z) const {
    if (x < 0 || y < 0 || z < 0 || x >= SIZE_X || y >= SIZE_Y || z >= SIZE_Z) return nullptr;
    return &m_data.voxels[ChunkData::index(x, y, z)];
}

const ChunkCoord &Chunk::getCoord() const {
    return m_data.coord;
}

void Chunk::setCoord(const ChunkCoord &coord) {
    m_data.coord = coord;
}

uint32_t Chunk::getVersion() const {
    return m_data.version;
}

bool Chunk::hasView() const {
    return m_view.terrain.mesh.id != BAMBOO_INVALID_HANDLE || m_view.blocks.mesh.id != BAMBOO_INVALID_HANDLE;
}

void Chunk::applyMaterials(MaterialHandle terrainMaterial, MaterialHandle blocksMaterial) {
    if (m_view.terrain.entity.isValid() && terrainMaterial.isValid()) {
        MeshComponentAPI::setMaterial(m_view.terrain.entity, terrainMaterial);
    }
    if (m_view.blocks.entity.isValid() && blocksMaterial.isValid()) {
        MeshComponentAPI::setMaterial(m_view.blocks.entity, blocksMaterial);
    }
}

std::size_t Chunk::getVertexCount() const {
    return m_data.vertexCount;
}

std::size_t Chunk::getIndexCount() const {
    return m_data.indexCount;
}

bool Chunk::needsRemesh() const {
    return m_data.needsRemesh;
}

bool Chunk::isModifiedByPlayer() const {
    return m_data.modifiedByPlayer;
}

bool Chunk::isMeshBuildQueued() const {
    return m_data.meshBuildQueued;
}

void Chunk::setMeshBuildQueued(bool queued) {
    m_data.meshBuildQueued = queued;
}

void Chunk::clearNeedsRemesh() {
    m_data.needsRemesh = false;
}

void Chunk::updateMeshes(const MeshData &terrainMesh, const MeshData &blocksMesh) {
    updateLayer(m_view.terrain, terrainMesh, "Chunk Terrain");
    updateLayer(m_view.blocks, blocksMesh, "Chunk Blocks");
    m_data.meshUploaded = hasView();
    m_data.vertexCount = terrainMesh.vertices.size() + blocksMesh.vertices.size();
    m_data.indexCount = terrainMesh.indices.size() + blocksMesh.indices.size();
}

void Chunk::clearMesh() {
    destroyView();
    m_data.needsRemesh = true;
}

ChunkData &Chunk::data() {
    return m_data;
}

const ChunkData &Chunk::data() const {
    return m_data;
}

ChunkView &Chunk::view() {
    return m_view;
}

const ChunkView &Chunk::view() const {
    return m_view;
}
