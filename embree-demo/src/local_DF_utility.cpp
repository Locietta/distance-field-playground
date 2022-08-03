#include "local_DF_utility.h"
#include "arg_parser.h"
#include "embree_wrapper.h"
#include "geometry_math.h"
#include "mesh.h"

#include <execution>
#include <glm/geometric.hpp>

constexpr glm::uint8 MAX_UINT8 = std::numeric_limits<glm::uint8>::max();
constexpr glm::uint8 MIN_UINT8 = std::numeric_limits<glm::uint8>::min();

DistanceFieldBrickTask::DistanceFieldBrickTask(embree::Scene const &embreeScene, std::vector<glm::vec3> const &sampleDirection,
                                               float localSpaceTraceDistance, Box volumeBounds, glm::uvec3 brickCoordinate,
                                               glm::vec3 indirectionVoxelSize)
    : embreeScene{embreeScene}, sampleDirection{sampleDirection}, localSpaceTraceDistance{localSpaceTraceDistance},
      volumeBounds{volumeBounds}, brickCoordinate{brickCoordinate}, indirectionVoxelSize{indirectionVoxelSize}, brickMaxDistance{MIN_UINT8},
      brickMinDistance{MAX_UINT8} {}

void DistanceFieldBrickTask::doWork() {
    const glm::vec3 distance_field_voxel_size = indirectionVoxelSize / (float) DistanceField::UniqueDataBrickSize;
    const glm::vec3 brick_min_position = volumeBounds.min + glm::vec3(brickCoordinate) * indirectionVoxelSize;

    embree::ClosestQueryContext point_query{embreeScene};
    embree::IntersectionContext intersect{embreeScene};

    distanceFieldVolume.resize((std::size_t) DistanceField::BrickSize * DistanceField::BrickSize * DistanceField::BrickSize);

    for (glm::uint z_index = 0; z_index < DistanceField::BrickSize; ++z_index) {
        for (glm::uint y_index = 0; y_index < DistanceField::BrickSize; ++y_index) {
            for (glm::uint x_index = 0; x_index < DistanceField::BrickSize; ++x_index) {
                const glm::vec3 sample_position = glm::vec3(x_index, y_index, z_index) * distance_field_voxel_size + brick_min_position;
                const glm::uint32 index =
                    z_index * DistanceField::BrickSize * DistanceField::BrickSize + y_index * DistanceField::BrickSize + x_index;

                float closest_distance = point_query.queryDistance(sample_position, 2 * localSpaceTraceDistance);

                if (closest_distance <= localSpaceTraceDistance) { // only trace rays for valid distance
                    int hit_back_count = 0;

                    for (const glm::vec3 unit_ray_direction : sampleDirection) {
                        const float pullback_epsilon = 1e-4f;
                        const glm::vec3 start_pos = sample_position - pullback_epsilon * localSpaceTraceDistance * unit_ray_direction;

                        // TODO: test ray intersect with bounding first
                        embree::RayHit rayhit = intersect.emitRay(start_pos, unit_ray_direction, localSpaceTraceDistance);

                        if (rayhit.isValidHit()) {
                            const glm::vec3 hit_normal = rayhit.getHitNormal();
                            if (glm::dot(unit_ray_direction, hit_normal) > 0) {
                                hit_back_count++;
                            }
                        }
                    }

                    if (hit_back_count && hit_back_count > sampleDirection.size() / 4) {
                        // consider it inside if significant ray hit back
                        closest_distance *= -1;
                    }
                }

                const float rescaled_distance = (closest_distance + localSpaceTraceDistance) / (2 * localSpaceTraceDistance);
                const glm::uint8 quantized_distance = glm::clamp((glm::uint32) glm::round(rescaled_distance * 255.0f), 0u, 255u);

                distanceFieldVolume[index] = quantized_distance;
                brickMinDistance = glm::min(brickMinDistance, quantized_distance);
                brickMaxDistance = glm::max(brickMaxDistance, quantized_distance);
            }
        }
    }
}

template <typename T> // T should be a STL container
constexpr std::size_t elementSize(T const &) {
    return sizeof(typename T::value_type);
}

constexpr float maxComponent(glm::vec3 vec) {
    return std::max(vec.x, std::max(vec.y, vec.z));
}

inline glm::uint computeLinearVoxelIndex(glm::uvec3 voxelCoordinate, glm::uvec3 volumeDimensions) {
    return (voxelCoordinate.z * volumeDimensions.y + voxelCoordinate.y) * volumeDimensions.x + voxelCoordinate.x;
}

extern ArgParser &arg_parser;

void generateDistanceFieldVolumeData(Mesh const &mesh, Box localSpaceMeshBounds, float distanceFieldResolutionScale,
                                     DistanceFieldVolumeData &outData) {
    embree::Scene embree_scene;
    embree_scene.setVertices(mesh.vertices);
    embree_scene.setIndices(mesh.indices);
    embree_scene.commit();

    std::vector<glm::vec3> sample_directions;
    {
        const int num_voxel_distance_samples = 49;
        sample_directions = stratifiedUniformHemisphereSamples(num_voxel_distance_samples);
        std::vector<glm::vec3> other_half_samples = stratifiedUniformHemisphereSamples(num_voxel_distance_samples);
        for (const auto &other_half_sample : other_half_samples) {
            sample_directions.emplace_back(other_half_sample.x, other_half_sample.y, -other_half_sample.z);
        }
    }

    { // ensure minimal 1x1x1 bounds to handle planes
        glm::vec3 mesh_bound_center = localSpaceMeshBounds.getCenter();
        glm::vec3 mesh_bound_extent = glm::max(localSpaceMeshBounds.getExtent(), glm::vec3(1.0f, 1.0f, 1.0f));
        localSpaceMeshBounds.min = mesh_bound_center - mesh_bound_extent;
        localSpaceMeshBounds.max = mesh_bound_center + mesh_bound_extent;
    }

    {
        /// NOTE: should expand bounds for 2-sided material
    }

    const float local_to_volume_scale = 1.0f / maxComponent(localSpaceMeshBounds.getExtent());

    const float num_voxel_per_local = arg_parser.voxel_density * distanceFieldResolutionScale;

    const glm::vec3 desired_dimensions = localSpaceMeshBounds.getSize() * (num_voxel_per_local / DistanceField::UniqueDataBrickSize);

    const glm::uvec3 mip0_indirection_dimensions =
        glm::clamp((glm::uvec3) glm::round(desired_dimensions), 1u, DistanceField::MaxIndirectionDimension);

    // TODO: mip? mip loop should start here
    const glm::uvec3 indirection_dimensions = mip0_indirection_dimensions;

    const glm::vec3 texel_size = localSpaceMeshBounds.getSize() / glm::vec3(indirection_dimensions * DistanceField::UniqueDataBrickSize -
                                                                            2 * DistanceField::MeshDistanceFieldObjectBorder);
    const Box distance_field_volume_bounds = localSpaceMeshBounds.expandBy(texel_size);
    const glm::vec3 indirection_voxel_size = distance_field_volume_bounds.getSize() / glm::vec3(indirection_dimensions);

    const float distance_field_voxel_size = glm::length(indirection_voxel_size) / DistanceField::UniqueDataBrickSize;
    const float local_space_trace_distance = distance_field_voxel_size * DistanceField::BandSizeInVoxels;
    const float volume_space_max_encoding = local_space_trace_distance * local_to_volume_scale;

    std::vector<DistanceFieldBrickTask> brick_tasks;
    brick_tasks.reserve(indirection_dimensions.x * indirection_dimensions.y * indirection_dimensions.z / 8);

    for (glm::uint z_index = 0; z_index < indirection_dimensions.z; ++z_index) {
        for (glm::uint y_index = 0; y_index < indirection_dimensions.y; ++y_index) {
            for (glm::uint x_index = 0; x_index < indirection_dimensions.x; ++x_index) {
                brick_tasks.emplace_back(embree_scene, sample_directions, local_space_trace_distance, distance_field_volume_bounds,
                                         glm::uvec3{x_index, y_index, z_index}, indirection_voxel_size);
            }
        }
    }

    if (arg_parser.parallel) {
        std::for_each(std::execution::par_unseq, brick_tasks.begin(), brick_tasks.end(),
                      [](DistanceFieldBrickTask &task) { task.doWork(); });

    } else {
        std::for_each(brick_tasks.begin(), brick_tasks.end(), [](DistanceFieldBrickTask &task) { task.doWork(); });
    }

    SparseDistanceFieldMip &out_mip = outData.mips[0];
    std::vector<glm::uint32> indirection_table;
    indirection_table.resize((std::size_t) indirection_dimensions.x * indirection_dimensions.y * indirection_dimensions.z,
                             DistanceField::InvalidBrickIndex);

    std::vector<DistanceFieldBrickTask *> valid_bricks;
    valid_bricks.reserve(brick_tasks.size());

    for (auto &brick_task : brick_tasks) {
        if (brick_task.brickMaxDistance > MIN_UINT8 && brick_task.brickMinDistance < MAX_UINT8) {
            valid_bricks.push_back(&brick_task);
        }
    }

    const glm::uint num_bricks = valid_bricks.size();
    const glm::uint brick_size_bytes =
        DistanceField::BrickSize * DistanceField::BrickSize * DistanceField::BrickSize * 1; // GPixelFormats[G8].BlockBytes == 1

    std::vector<glm::uint8> distance_field_brick_data;
    /// NOTE: un-inited in UE5
    distance_field_brick_data.resize((std::size_t) num_bricks * brick_size_bytes);

    for (std::size_t brick_index = 0; brick_index < valid_bricks.size(); ++brick_index) {
        const DistanceFieldBrickTask &brick = *valid_bricks[brick_index];
        const glm::uint indirection_index = computeLinearVoxelIndex(brick.brickCoordinate, indirection_dimensions);
        indirection_table[indirection_index] = brick_index;

        assert(brick_size_bytes == brick.distanceFieldVolume.size() * elementSize(brick.distanceFieldVolume));
        std::memcpy(&distance_field_brick_data[brick_index * brick_size_bytes], brick.distanceFieldVolume.data(), brick_size_bytes);
    }

    const glm::uint indirection_table_bytes = indirection_table.size() * elementSize(indirection_table);
    const glm::uint mip_data_bytes = indirection_table_bytes + distance_field_brick_data.size();

    { // always loaded
        outData.alwaysLoadedMip.resize(mip_data_bytes);

        std::memcpy(&outData.alwaysLoadedMip[0], indirection_table.data(), indirection_table_bytes);
        if (distance_field_brick_data.size() > 0) {
            std::memcpy(&outData.alwaysLoadedMip[indirection_table_bytes], distance_field_brick_data.data(),
                        distance_field_brick_data.size());
        }
    }

    // TODO: other 2 level of mips is streamed

    out_mip.indirectionDimensions = indirection_dimensions;
    out_mip.distanceFieldToVolumeScaleBias = glm::vec2{2 * volume_space_max_encoding, -volume_space_max_encoding};
    out_mip.numDistanceFieldBricks = num_bricks;

    const glm::vec3 virtual_UV_min =
        glm::vec3(DistanceField::MeshDistanceFieldObjectBorder) / glm::vec3(indirection_dimensions * DistanceField::UniqueDataBrickSize);
    const glm::vec3 virtual_UV_size =
        glm::vec3(indirection_dimensions * DistanceField::UniqueDataBrickSize - 2 * DistanceField::MeshDistanceFieldObjectBorder) /
        glm::vec3(indirection_dimensions * DistanceField::UniqueDataBrickSize);

    const glm::vec3 volume_space_extent = localSpaceMeshBounds.getExtent() * local_to_volume_scale;

    out_mip.volumeToVirtualUVScale = virtual_UV_size / (2.0f * volume_space_extent);
    out_mip.volumeToVirtualUVAdd = volume_space_extent * out_mip.volumeToVirtualUVScale + virtual_UV_min;

    /// NOTE: mip loop end here

    outData.localSpaceMeshBounds = localSpaceMeshBounds;
}