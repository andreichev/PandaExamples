//
// Created by Admin on 12.02.2022.
//

#include "Chunk.hpp"

#include <Bamboo/Assets/AssetManagerAPI.hpp>
#include <Bamboo/WorldAPI.hpp>
#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/Components/MeshComponentAPI.hpp>

int ChunkData::index(int x, int y, int z) {
    return y * SIZE_X * SIZE_Z + x * SIZE_Z + z;
}

Chunk::~Chunk() {
    AssetManagerAPI::deleteMesh(m_view.mesh);
    WorldAPI::destroyEntity(m_view.entity);
}

Chunk::Chunk() {
    m_view.mesh = AssetManagerAPI::createMesh();
    m_view.entity = WorldAPI::createEntity("Chunk");
    EntityAPI::addComponent(m_view.entity, ComponentType::MESH_COMPONENT);
    MeshComponentAPI::setMesh(m_view.entity, m_view.mesh);
}

void Chunk::set(int x, int y, int z, VoxelType type) {
    if (x < 0 || y < 0 || z < 0 || x >= SIZE_X || y >= SIZE_Y || z >= SIZE_Z) return;
    Voxel &voxel = m_data.voxels[ChunkData::index(x, y, z)];
    if (voxel.type == type) { return; }
    voxel.type = type;
    m_data.version++;
    m_data.needsRemesh = true;
    m_data.modifiedByPlayer = true;
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

bool Chunk::needsRemesh() const {
    return m_data.needsRemesh;
}

void Chunk::clearNeedsRemesh() {
    m_data.needsRemesh = false;
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
