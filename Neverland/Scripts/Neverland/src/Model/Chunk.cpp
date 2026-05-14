//
// Created by Admin on 12.02.2022.
//

#include "Chunk.hpp"

#include <Bamboo/Assets/AssetManagerAPI.hpp>
#include <Bamboo/WorldAPI.hpp>
#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/Components/MeshComponentAPI.hpp>

Chunk::~Chunk() {
    AssetManagerAPI::deleteMesh(m_mesh);
    WorldAPI::destroyEntity(m_entity);
    delete[] m_data;
}

Chunk::Chunk() {
    m_data = new Voxel[SIZE_X * SIZE_Y * SIZE_Z];
    m_mesh = AssetManagerAPI::createMesh();
    m_entity = WorldAPI::createEntity("Chunk");
    EntityAPI::addComponent(m_entity, ComponentType::MESH_COMPONENT);
    MeshComponentAPI::setMesh(m_entity, m_mesh);
}

void Chunk::set(int x, int y, int z, VoxelType type) {
    if (x < 0 || y < 0 || z < 0 || x >= SIZE_X || y >= SIZE_Y || z >= SIZE_Z) return;
    m_data[y * Chunk::SIZE_X * Chunk::SIZE_Z + x * Chunk::SIZE_X + z].type = type;
}

Voxel *Chunk::get(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= SIZE_X || y >= SIZE_Y || z >= SIZE_Z) return nullptr;
    return &m_data[y * Chunk::SIZE_X * Chunk::SIZE_Z + x * Chunk::SIZE_X + z];
}

MeshHandle Chunk::getMesh() {
    return m_mesh;
}

EntityHandle Chunk::getEntity() {
    return m_entity;
}
