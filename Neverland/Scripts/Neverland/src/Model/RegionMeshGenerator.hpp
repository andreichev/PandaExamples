#pragma once

#include "RegionMesh.hpp"

class RegionMeshGenerator final {
public:
    static RegionMeshBuildResult makeRegionMesh(const RegionMeshBuildRequest &request);
};
