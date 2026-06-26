//
// Created by Admin on 12.02.2022.
//

#pragma once

#include "Voxel.hpp"

#include <Bamboo/Assets/MeshAPI.hpp>
#include <Bamboo/Base.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

using namespace Bamboo;

struct ChunkCoord {
    int x = 0;
    int y = 0;
    int z = 0;
};

inline bool operator==(const ChunkCoord &left, const ChunkCoord &right) {
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

struct ChunkCoordHash {
    std::size_t operator()(const ChunkCoord &coord) const noexcept;
};

struct ChunkData {
    static constexpr int SIZE_X = 20;
    static constexpr int SIZE_Y = 20;
    static constexpr int SIZE_Z = 20;
    static constexpr int VOXEL_COUNT = SIZE_X * SIZE_Y * SIZE_Z;

    ChunkCoord coord;
    std::array<Voxel, VOXEL_COUNT> voxels;
    uint32_t version = 0;
    bool needsRemesh = true;
    bool modifiedByPlayer = false;
    bool meshBuildQueued = false;
    bool meshUploaded = false;
    std::size_t vertexCount = 0;
    std::size_t indexCount = 0;

    static int index(int x, int y, int z);
};

struct ChunkView {
    EntityHandle entity = 0;
    MeshHandle mesh = 0;
};

class Chunk {
public:
    static constexpr int SIZE_X = ChunkData::SIZE_X;
    static constexpr int SIZE_Y = ChunkData::SIZE_Y;
    static constexpr int SIZE_Z = ChunkData::SIZE_Z;
    static constexpr int VOXEL_COUNT = ChunkData::VOXEL_COUNT;

    Chunk();
    ~Chunk();

    Chunk(const Chunk &) = delete;
    Chunk &operator=(const Chunk &) = delete;

    MeshHandle getMesh() const;
    EntityHandle getEntity() const;
    const ChunkCoord &getCoord() const;
    void setCoord(const ChunkCoord &coord);
    uint32_t getVersion() const;
    bool hasView() const;
    std::size_t getVertexCount() const;
    std::size_t getIndexCount() const;
    bool needsRemesh() const;
    bool isModifiedByPlayer() const;
    bool isMeshBuildQueued() const;
    void setMeshBuildQueued(bool queued);
    void clearNeedsRemesh();
    void updateMesh(const MeshData &meshData);
    void clearMesh();

    bool set(int x, int y, int z, VoxelType type);
    Voxel *get(int x, int y, int z);
    const Voxel *get(int x, int y, int z) const;
    ChunkData &data();
    const ChunkData &data() const;
    ChunkView &view();
    const ChunkView &view() const;

private:
    void ensureView();
    void destroyView();

    ChunkData m_data;
    ChunkView m_view;
};
