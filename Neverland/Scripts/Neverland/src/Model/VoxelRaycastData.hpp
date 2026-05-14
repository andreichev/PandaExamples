//
// Created by Admin on 12.02.2022.
//

#pragma once

#include "Voxel.hpp"

struct VoxelRaycastData {
    Voxel *voxel;
    Vec3 end;
    Vec3 normal;

    VoxelRaycastData(Voxel *voxel, const Vec3 &anEnd, const Vec3 &normal)
        : voxel(voxel)
        , end(anEnd)
        , normal(normal) {}
};
