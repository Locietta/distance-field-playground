#pragma once

#include "mesh.h"

#include <array>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <vector>

namespace DistanceField {

constexpr glm::uint32 UniqueDataBrickSize = 7;

constexpr glm::uint32 BrickSize = 8;

constexpr glm::uint32 BandSizeInVoxels = 4; // brick radius

constexpr glm::uint32 InvalidBrickIndex = 0xFFFFFFFF;

constexpr glm::uint32 MaxIndirectionDimension = 1024;

constexpr glm::uint32 MeshDistanceFieldObjectBorder = 1;

constexpr glm::uint32 NumMips = 3;

} // namespace DistanceField

// -------------------- Forward Declarations ---------------------

namespace embree {
class Scene;
}

class DistanceFieldBrickTask {
public:
    DistanceFieldBrickTask(embree::Scene const &embreeScene, std::vector<glm::vec3> const &sampleDirection, float localSpaceTraceDistance,
                           Box volumeBounds, glm::uvec3 brickCoordinate, glm::vec3 indirectionVoxelSize);

    void doWork();

    // input, read-only
    embree::Scene const &embreeScene;
    std::vector<glm::vec3> const &sampleDirection;
    float localSpaceTraceDistance;
    Box volumeBounds;
    const glm::uvec3 brickCoordinate;
    const glm::vec3 indirectionVoxelSize;

    // outputs
    glm::uint8 brickMaxDistance;
    glm::uint8 brickMinDistance;
    std::vector<glm::uint8> distanceFieldVolume;
};

class SparseDistanceFieldMip {
public:
    glm::uvec3 indirectionDimensions;
    glm::uint32 numDistanceFieldBricks;
    glm::vec3 volumeToVirtualUVScale;
    glm::vec3 volumeToVirtualUVAdd;
    glm::vec2 distanceFieldToVolumeScaleBias;

    glm::uint32 bulkOffset;
    glm::uint32 bulkSize;
};

/// defered clean-up resource
class DistanceFieldVolumeData {
public:
    Box localSpaceMeshBounds;

    bool bMostlyTwoSided;

    std::array<SparseDistanceFieldMip, DistanceField::NumMips> mips;

    std::vector<glm::uint8> alwaysLoadedMip;

    // TODO: need to switch to streaming bulk in Chaos
    std::vector<glm::uint8> streamableMips;
};

/// NOTE: part of FMeshUtilities in ue5
void generateDistanceFieldVolumeData(Mesh const &mesh, Box bounds, float distanceFieldResolutionScale, DistanceFieldVolumeData &outData);