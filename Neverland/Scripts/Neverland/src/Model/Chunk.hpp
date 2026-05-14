//
// Created by Admin on 12.02.2022.
//

#pragma once

#include "Voxel.hpp"
#include <Bamboo/Base.hpp>

using namespace Bamboo;

class Chunk {
public:
    static const int SIZE_X = 20;
    static const int SIZE_Y = 20;
    static const int SIZE_Z = 20;

    Chunk();
    ~Chunk();
    MeshHandle getMesh();
    EntityHandle getEntity();
    void set(int x, int y, int z, VoxelType type);
    Voxel *get(int x, int y, int z);
    Voxel *m_data;

private:
    EntityHandle m_entity;
    MeshHandle m_mesh;
};
