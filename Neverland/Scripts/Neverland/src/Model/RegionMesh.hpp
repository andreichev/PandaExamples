#pragma once

#include <Bamboo/Assets/MeshAPI.hpp>
#include <Bamboo/Base.hpp>

#include <cstddef>
#include <cstdint>

using namespace Bamboo;

struct RegionMeshKey {
    int x = 0;
    int z = 0;
    int lod = 0;
};

inline bool operator==(const RegionMeshKey &left, const RegionMeshKey &right) {
    return left.x == right.x && left.z == right.z && left.lod == right.lod;
}

struct RegionMeshKeyHash {
    std::size_t operator()(const RegionMeshKey &key) const noexcept;
};

struct RegionMeshBuildRequest {
    RegionMeshKey key;
    int regionSizeChunks = 1;
    int sampleStepBlocks = 1;
    int minChunkDistance = 0;
    int maxChunkDistance = 0;
    int centerChunkX = 0;
    int centerChunkZ = 0;
    uint64_t requestId = 0;
};

struct RegionMeshBuildResult {
    RegionMeshKey key;
    uint64_t requestId = 0;
    MeshData meshData;
};

struct RegionMeshView {
    EntityHandle entity = 0;
    MeshHandle mesh = 0;
};

class RegionMesh {
public:
    RegionMesh();
    ~RegionMesh();

    RegionMesh(const RegionMesh &) = delete;
    RegionMesh &operator=(const RegionMesh &) = delete;

    void setKey(const RegionMeshKey &key);
    const RegionMeshKey &getKey() const;
    bool hasView() const;
    bool needsRemesh() const;
    bool isMeshBuildQueued() const;
    uint64_t getRequestId() const;
    std::size_t getVertexCount() const;
    std::size_t getIndexCount() const;
    EntityHandle getEntity() const;

    void setDesiredRequest(uint64_t requestId);
    void setMeshBuildQueued(bool queued);
    void clearNeedsRemesh();
    void updateMesh(const MeshData &meshData);
    void clearMesh();

private:
    void ensureView();
    void destroyView();

    RegionMeshKey m_key;
    RegionMeshView m_view;
    uint64_t m_requestId = 0;
    bool m_needsRemesh = true;
    bool m_meshBuildQueued = false;
    std::size_t m_vertexCount = 0;
    std::size_t m_indexCount = 0;
};
