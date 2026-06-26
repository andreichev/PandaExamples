#include "RegionMesh.hpp"

#include <Bamboo/Assets/AssetManagerAPI.hpp>
#include <Bamboo/WorldAPI.hpp>
#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/Components/MeshComponentAPI.hpp>

std::size_t RegionMeshKeyHash::operator()(const RegionMeshKey &key) const noexcept {
    std::size_t seed = 0;
    auto combine = [&seed](int value) {
        std::size_t hash = static_cast<std::size_t>(value);
        seed ^= hash + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    };
    combine(key.x);
    combine(key.z);
    combine(key.lod);
    return seed;
}

RegionMesh::RegionMesh() = default;

RegionMesh::~RegionMesh() {
    destroyView();
}

void RegionMesh::setKey(const RegionMeshKey &key) {
    m_key = key;
}

const RegionMeshKey &RegionMesh::getKey() const {
    return m_key;
}

bool RegionMesh::hasView() const {
    return m_view.entity.id != BAMBOO_INVALID_HANDLE && m_view.mesh.id != BAMBOO_INVALID_HANDLE;
}

bool RegionMesh::needsRemesh() const {
    return m_needsRemesh;
}

bool RegionMesh::isMeshBuildQueued() const {
    return m_meshBuildQueued;
}

uint64_t RegionMesh::getRequestId() const {
    return m_requestId;
}

std::size_t RegionMesh::getVertexCount() const {
    return m_vertexCount;
}

std::size_t RegionMesh::getIndexCount() const {
    return m_indexCount;
}

EntityHandle RegionMesh::getEntity() const {
    return m_view.entity;
}

void RegionMesh::setDesiredRequest(uint64_t requestId) {
    if (m_requestId == requestId) { return; }
    m_requestId = requestId;
    m_needsRemesh = true;
}

void RegionMesh::setMeshBuildQueued(bool queued) {
    m_meshBuildQueued = queued;
}

void RegionMesh::clearNeedsRemesh() {
    m_needsRemesh = false;
}

void RegionMesh::updateMesh(const MeshData &meshData) {
    if (meshData.vertices.empty() || meshData.indices.empty()) {
        clearMesh();
        return;
    }

    ensureView();
    MeshAPI::update(m_view.mesh, meshData);
    m_vertexCount = meshData.vertices.size();
    m_indexCount = meshData.indices.size();
}

void RegionMesh::clearMesh() {
    destroyView();
}

void RegionMesh::ensureView() {
    if (hasView()) { return; }
    m_view.mesh = AssetManagerAPI::createMesh();
    m_view.entity = WorldAPI::createEntity("Region LOD");
    EntityAPI::addComponent(m_view.entity, ComponentType::MESH_COMPONENT);
    MeshComponentAPI::setMesh(m_view.entity, m_view.mesh);
}

void RegionMesh::destroyView() {
    if (m_view.mesh.isValid()) {
        AssetManagerAPI::deleteMesh(m_view.mesh);
        m_view.mesh = {};
    }
    if (m_view.entity.isValid()) {
        WorldAPI::destroyEntity(m_view.entity);
        m_view.entity = {};
    }
    m_vertexCount = 0;
    m_indexCount = 0;
}
