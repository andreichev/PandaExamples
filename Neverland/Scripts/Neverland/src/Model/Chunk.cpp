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

void Chunk::ensureView() {
    if (hasView()) { return; }
    m_view.mesh = AssetManagerAPI::createMesh();
    m_view.entity = WorldAPI::createEntity("Chunk");
    EntityAPI::addComponent(m_view.entity, ComponentType::MESH_COMPONENT);
    MeshComponentAPI::setMesh(m_view.entity, m_view.mesh);
}

void Chunk::destroyView() {
    if (m_view.mesh.isValid()) {
        AssetManagerAPI::deleteMesh(m_view.mesh);
        m_view.mesh = {};
    }
    if (m_view.entity.isValid()) {
        WorldAPI::destroyEntity(m_view.entity);
        m_view.entity = {};
    }
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

MeshHandle Chunk::getMesh() const {
    return m_view.mesh;
}

EntityHandle Chunk::getEntity() const {
    return m_view.entity;
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
    return m_view.entity.id != BAMBOO_INVALID_HANDLE && m_view.mesh.id != BAMBOO_INVALID_HANDLE;
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

void Chunk::updateMesh(const MeshData &meshData) {
    if (meshData.vertices.empty() || meshData.indices.empty()) {
        clearMesh();
        return;
    }
    ensureView();
    MeshAPI::update(m_view.mesh, meshData);
    m_data.meshUploaded = true;
    m_data.vertexCount = meshData.vertices.size();
    m_data.indexCount = meshData.indices.size();
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
