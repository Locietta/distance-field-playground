#pragma once

#include "mesh.h"

#include <array>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <istream>
#include <ostream>
#include <vector>

namespace DistanceField {

constexpr glm::uint32 UNIQUE_DATA_BRICK_SIZE = 7;

constexpr glm::uint32 BRICK_SIZE = 8;

constexpr glm::uint32 BAND_SIZE_IN_VOXELS = 4; // brick radius

constexpr glm::uint32 INVALID_BRICK_INDEX = 0xFFFFFFFF;

constexpr glm::uint32 MAX_INDIRECTION_DIMENSION = 1024;

constexpr glm::uint32 MESH_DISTANCE_FIELD_OBJECT_BORDER = 1;

constexpr glm::uint32 NUM_MIPS = 3;

} // namespace DistanceField

// -------------------- Forward Declarations ---------------------

namespace embree {
class Scene;
}

class DistanceFieldBrickTask {
public:
    DistanceFieldBrickTask(embree::Scene const &embree_scene, std::vector<glm::vec3> const &sample_direction,
                           float local_space_trace_distance, Box volume_bounds, glm::uvec3 brick_coordinate,
                           glm::vec3 indirection_voxel_size);

    void doWork();

    // input, read-only
    embree::Scene const &embree_scene;
    std::vector<glm::vec3> const &sample_direction;
    float local_space_trace_distance;
    Box volume_bounds;
    const glm::uvec3 brick_coordinate;
    const glm::vec3 indirection_voxel_size;

    // outputs
    glm::uint8 brick_max_distance;
    glm::uint8 brick_min_distance;
    std::vector<glm::uint8> distance_field_volume;
};

struct SparseDistanceFieldMip {
    glm::uvec3 indirection_dimensions;
    glm::uint32 num_distance_field_bricks;
    glm::vec3 volume_to_virtual_uv_scale;
    glm::vec3 volume_to_virtual_uv_add;
    glm::vec2 distance_field_to_volume_scale_bias;

    glm::uint32 bulk_offset;
    glm::uint32 bulk_size;
};

/// defered clean-up resource
class DistanceFieldVolumeData {
public:
    Box local_space_mesh_bounds;

    // bool bMostlyTwoSided;

    std::array<SparseDistanceFieldMip, DistanceField::NUM_MIPS> mips;

    std::vector<glm::uint8> always_loaded_mip;

    // XXX: need to switch to streaming bulk in Chaos
    std::vector<glm::uint8> streamable_mips;

    static void serialize(std::ostream &os, DistanceFieldVolumeData const &data);
    static void deserialize(std::istream &is, DistanceFieldVolumeData &data);
};

/// NOTE: part of FMeshUtilities in ue5
void generate_distance_field_volume_data(Mesh const &mesh, Box bounds, float distance_field_resolution_scale,
                                         DistanceFieldVolumeData &out_data);