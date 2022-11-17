#include "local_DF_utility.h"
#include "arg_parser.h"
#include "embree_wrapper.h"
#include "geometry_math.h"
#include "mesh.h"

#include <chrono>
#include <execution>
#include <fmt/core.h>
#include <glm/geometric.hpp>

constexpr glm::uint8 MAX_UINT8 = std::numeric_limits<glm::uint8>::max();
constexpr glm::uint8 MIN_UINT8 = std::numeric_limits<glm::uint8>::min();

DistanceFieldBrickTask::DistanceFieldBrickTask(embree::Scene const &embree_scene, std::span<const glm::vec3> sample_direction,
                                               float local_space_trace_distance, Box volume_bounds, glm::uvec3 brick_coordinate,
                                               glm::vec3 indirection_voxel_size)
    : embree_scene{embree_scene}, sample_direction{sample_direction}, local_space_trace_distance{local_space_trace_distance},
      volume_bounds{volume_bounds}, brick_coordinate{brick_coordinate}, indirection_voxel_size{indirection_voxel_size},
      brick_max_distance{MIN_UINT8}, brick_min_distance{MAX_UINT8} {}

void DistanceFieldBrickTask::doWork() {
    const glm::vec3 distance_field_voxel_size = indirection_voxel_size / (float) DistanceField::UNIQUE_DATA_BRICK_SIZE;
    const glm::vec3 brick_min_position = volume_bounds.min + glm::vec3(brick_coordinate) * indirection_voxel_size;

    embree::ClosestQueryContext point_query{embree_scene};
    embree::IntersectionContext intersect{embree_scene};

    distance_field_volume.resize((std::size_t) DistanceField::BRICK_SIZE * DistanceField::BRICK_SIZE * DistanceField::BRICK_SIZE);

    for (glm::uint z_index = 0; z_index < DistanceField::BRICK_SIZE; ++z_index) {
        for (glm::uint y_index = 0; y_index < DistanceField::BRICK_SIZE; ++y_index) {
            for (glm::uint x_index = 0; x_index < DistanceField::BRICK_SIZE; ++x_index) {
                const glm::vec3 sample_position = glm::vec3(x_index, y_index, z_index) * distance_field_voxel_size + brick_min_position;
                const glm::uint32 index =
                    z_index * DistanceField::BRICK_SIZE * DistanceField::BRICK_SIZE + y_index * DistanceField::BRICK_SIZE + x_index;

                float closest_distance = point_query.queryDistance(sample_position, 1.5f * local_space_trace_distance);

                if (closest_distance <= local_space_trace_distance) { // only trace rays for valid distance
                    int hit_back_count = 0;

                    for (const glm::vec3 unit_ray_direction : sample_direction) {
                        const float pullback_epsilon = 1e-4f;
                        const glm::vec3 start_pos = sample_position - pullback_epsilon * local_space_trace_distance * unit_ray_direction;

                        // TODO: test ray intersect with bounding first
                        embree::RayHit rayhit = intersect.emitRay(start_pos, unit_ray_direction, local_space_trace_distance);

                        if (rayhit.isValidHit()) {
                            const glm::vec3 hit_normal = rayhit.getHitNormal();
                            if (glm::dot(unit_ray_direction, hit_normal) > 0) {
                                hit_back_count++;
                            }
                        }
                    }

                    if (hit_back_count != 0 && hit_back_count > sample_direction.size() / 4) {
                        // consider it inside if significant ray hit back
                        closest_distance *= -1;
                    }
                }

                const float rescaled_distance = (closest_distance + local_space_trace_distance) / (2 * local_space_trace_distance);
                const glm::uint8 quantized_distance = glm::clamp((glm::uint32) glm::round(rescaled_distance * 255.0f), 0u, 255u);

                distance_field_volume[index] = quantized_distance;
                brick_min_distance = glm::min(brick_min_distance, quantized_distance);
                brick_max_distance = glm::max(brick_max_distance, quantized_distance);
            }
        }
    }
}

template <std::integral T>
T divide_and_round_up(T dividend, T divisor) {
    return (dividend + divisor - 1) / divisor;
}

template <typename T> // T should be a STL container
constexpr std::size_t element_size(T const & /*unused*/) {
    return sizeof(typename T::value_type);
}

constexpr float max_component(glm::vec3 vec) {
    return std::max(vec.x, std::max(vec.y, vec.z));
}

inline glm::uint compute_linear_voxel_index(glm::uvec3 voxel_coordinate, glm::uvec3 volume_dimensions) {
    return (voxel_coordinate.z * volume_dimensions.y + voxel_coordinate.y) * volume_dimensions.x + voxel_coordinate.x;
}

extern ArgParser &arg_parser;

void generate_distance_field_volume_data(Mesh const &mesh, Box local_space_mesh_bounds, float distance_field_resolution_scale,
                                         DistanceFieldVolumeData &out_data) {

    if (distance_field_resolution_scale <= 0) return; // sanity check

    auto start_time = std::chrono::steady_clock::now();

    embree::Scene embree_scene;
    embree_scene.addMesh(mesh);
    // embree_scene.addMesh(mesh.translate({1, 1, 1}));
    embree_scene.commit();

    auto scene_prepare_end_time = std::chrono::steady_clock::now();
    fmt::print("Prepare embree scene in {:.1f}s\n", std::chrono::duration<double>(scene_prepare_end_time - start_time).count());

    std::vector<glm::vec3> sample_directions;
    {
        const int num_voxel_distance_samples = 49;
        sample_directions = stratified_uniform_hemisphere_samples(num_voxel_distance_samples);
        std::vector<glm::vec3> other_half_samples = stratified_uniform_hemisphere_samples(num_voxel_distance_samples);
        for (const auto &other_half_sample : other_half_samples) {
            sample_directions.emplace_back(other_half_sample.x, other_half_sample.y, -other_half_sample.z);
        }
    }

    { // ensure minimal 1x1x1 bounds to handle planes
        glm::vec3 mesh_bound_center = local_space_mesh_bounds.getCenter();
        glm::vec3 mesh_bound_extent = glm::max(local_space_mesh_bounds.getExtent(), glm::vec3(1.0f, 1.0f, 1.0f));
        local_space_mesh_bounds.min = mesh_bound_center - mesh_bound_extent;
        local_space_mesh_bounds.max = mesh_bound_center + mesh_bound_extent;
    }

    {
        /// NOTE: expand bounds for 2-sided material
    }

    const float local_to_volume_scale = 1.0f / max_component(local_space_mesh_bounds.getExtent());

    const float num_voxel_per_local = arg_parser.voxel_density * distance_field_resolution_scale;

    const glm::vec3 desired_dimensions = local_space_mesh_bounds.getSize() * (num_voxel_per_local / DistanceField::UNIQUE_DATA_BRICK_SIZE);

    const glm::uvec3 mip0_indirection_dimensions =
        glm::clamp((glm::uvec3) glm::round(desired_dimensions), 1u, DistanceField::MAX_INDIRECTION_DIMENSION);

    std::vector<glm::uint8> streamable_mip_data;

    for (int mip_index = 0; mip_index < DistanceField::NUM_MIPS; ++mip_index) {
        const glm::uvec3 indirection_dimensions{
            divide_and_round_up(mip0_indirection_dimensions.x, 1u << mip_index),
            divide_and_round_up(mip0_indirection_dimensions.y, 1u << mip_index),
            divide_and_round_up(mip0_indirection_dimensions.z, 1u << mip_index),
        };

        const glm::vec3 texel_size =
            local_space_mesh_bounds.getSize() / glm::vec3(indirection_dimensions * DistanceField::UNIQUE_DATA_BRICK_SIZE -
                                                          2 * DistanceField::MESH_DISTANCE_FIELD_OBJECT_BORDER);
        const Box distance_field_volume_bounds = local_space_mesh_bounds.expandBy(texel_size);
        const glm::vec3 indirection_voxel_size = distance_field_volume_bounds.getSize() / glm::vec3(indirection_dimensions);

        const float distance_field_voxel_size = glm::length(indirection_voxel_size) / DistanceField::UNIQUE_DATA_BRICK_SIZE;
        const float local_space_trace_distance = distance_field_voxel_size * DistanceField::BAND_SIZE_IN_VOXELS;
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

        // XXX: use Async task mechanism in Chaos for parallel-for, if available
        if (arg_parser.parallel) {
            std::for_each(std::execution::par_unseq, brick_tasks.begin(), brick_tasks.end(),
                          [](DistanceFieldBrickTask &task) { task.doWork(); });

        } else {
            std::for_each(brick_tasks.begin(), brick_tasks.end(), [](DistanceFieldBrickTask &task) { task.doWork(); });
        }

        SparseDistanceFieldMip &out_mip = out_data.mips[mip_index];
        std::vector<glm::uint32> indirection_table;
        indirection_table.resize((std::size_t) indirection_dimensions.x * indirection_dimensions.y * indirection_dimensions.z,
                                 DistanceField::INVALID_BRICK_INDEX);

        std::vector<DistanceFieldBrickTask *> valid_bricks;
        valid_bricks.reserve(brick_tasks.size());

        for (auto &brick_task : brick_tasks) {
            if (brick_task.brick_max_distance > MIN_UINT8 && brick_task.brick_min_distance < MAX_UINT8) {
                valid_bricks.push_back(&brick_task);
            }
        }

        const glm::uint num_bricks = valid_bricks.size();
        const glm::uint brick_size_bytes =
            DistanceField::BRICK_SIZE * DistanceField::BRICK_SIZE * DistanceField::BRICK_SIZE * 1; // GPixelFormats[G8].BlockBytes == 1

        std::vector<glm::uint8> distance_field_brick_data;
        /// XXX: un-inited in UE5, vector<T>::resize will do zero-init
        distance_field_brick_data.resize((std::size_t) num_bricks * brick_size_bytes);

        for (std::size_t brick_index = 0; brick_index < valid_bricks.size(); ++brick_index) {
            const DistanceFieldBrickTask &brick = *valid_bricks[brick_index];
            const glm::uint indirection_index = compute_linear_voxel_index(brick.brick_coordinate, indirection_dimensions);
            indirection_table[indirection_index] = brick_index;

            assert(brick_size_bytes == brick.distance_field_volume.size() * element_size(brick.distance_field_volume));
            std::memcpy(&distance_field_brick_data[brick_index * brick_size_bytes], brick.distance_field_volume.data(), brick_size_bytes);
        }

        const glm::uint indirection_table_bytes = indirection_table.size() * element_size(indirection_table);
        const glm::uint mip_data_bytes = indirection_table_bytes + distance_field_brick_data.size();

        if (mip_index == DistanceField::NUM_MIPS - 1) {
            out_data.always_loaded_mip.resize(mip_data_bytes);
            out_mip.bulk_offset = out_mip.bulk_size = 0;

            std::memcpy(out_data.always_loaded_mip.data(), indirection_table.data(), indirection_table_bytes);
            if (!distance_field_brick_data.empty()) {
                std::memcpy(&out_data.always_loaded_mip[indirection_table_bytes], distance_field_brick_data.data(),
                            distance_field_brick_data.size());
            }
        } else {
            out_mip.bulk_offset = streamable_mip_data.size();
            streamable_mip_data.resize(streamable_mip_data.size() + mip_data_bytes);
            out_mip.bulk_size = mip_data_bytes;

            std::memcpy(&streamable_mip_data[out_mip.bulk_offset], indirection_table.data(), indirection_table_bytes);
            if (!distance_field_brick_data.empty()) {
                std::memcpy(&streamable_mip_data[out_mip.bulk_offset + indirection_table_bytes], distance_field_brick_data.data(),
                            distance_field_brick_data.size());
            }
        }

        out_mip.indirection_dimensions = indirection_dimensions;
        out_mip.distance_field_to_volume_scale_bias = glm::vec2{2 * volume_space_max_encoding, -volume_space_max_encoding};
        out_mip.num_distance_field_bricks = num_bricks;

        const glm::vec3 virtual_uv_min = glm::vec3(DistanceField::MESH_DISTANCE_FIELD_OBJECT_BORDER) /
                                         glm::vec3(indirection_dimensions * DistanceField::UNIQUE_DATA_BRICK_SIZE);
        const glm::vec3 virtual_uv_size = glm::vec3(indirection_dimensions * DistanceField::UNIQUE_DATA_BRICK_SIZE -
                                                    2 * DistanceField::MESH_DISTANCE_FIELD_OBJECT_BORDER) /
                                          glm::vec3(indirection_dimensions * DistanceField::UNIQUE_DATA_BRICK_SIZE);

        const glm::vec3 volume_space_extent = local_space_mesh_bounds.getExtent() * local_to_volume_scale;

        out_mip.volume_to_virtual_uv_scale = virtual_uv_size / (2.0f * volume_space_extent);
        out_mip.volume_to_virtual_uv_add = volume_space_extent * out_mip.volume_to_virtual_uv_scale + virtual_uv_min;
        fmt::print("Mip level {} compression: {}/{}\n", mip_index, valid_bricks.size(), brick_tasks.size());
    }

    out_data.local_space_mesh_bounds = local_space_mesh_bounds;
    out_data.streamable_mips = std::move(streamable_mip_data); // XXX: should use streaming bulk in Chaos

    auto end_time = std::chrono::steady_clock::now();
    fmt::print("Distance field calculation finished in {:.1f}s overall - {}x{}x{} sparse distance field.\n",
               std::chrono::duration<double>(end_time - start_time).count(),
               mip0_indirection_dimensions.x * DistanceField::UNIQUE_DATA_BRICK_SIZE,
               mip0_indirection_dimensions.y * DistanceField::UNIQUE_DATA_BRICK_SIZE,
               mip0_indirection_dimensions.z * DistanceField::UNIQUE_DATA_BRICK_SIZE);
}

#include "serializer.hpp"

void DistanceFieldVolumeData::serialize(std::ostream &os, DistanceFieldVolumeData const &data) {
    ::serialize(os, data.local_space_mesh_bounds);
    ::serialize(os, data.mips);
    ::serialize(os, data.always_loaded_mip);
    ::serialize(os, data.streamable_mips);
}

void DistanceFieldVolumeData::deserialize(std::istream &is, DistanceFieldVolumeData &data) {
    ::deserialize(is, data.local_space_mesh_bounds);
    ::deserialize(is, data.mips);
    ::deserialize(is, data.always_loaded_mip);
    ::deserialize(is, data.streamable_mips);
}
