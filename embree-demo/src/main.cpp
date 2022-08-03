#include "arg_parser.h"
#include "embree_wrapper.h"
#include "geometry_math.h"
#include "local_DF_utility.h"
#include "mesh.h"
#include "ply_loader.h"

#include "format.hpp"
#include <fmt/core.h>
#include <glm/common.hpp>

#define next_and_check(i)                                                                                                                  \
    (i)++;                                                                                                                                 \
    assert((i) < argc)

constexpr glm::uint8 MAX_UINT8 = std::numeric_limits<glm::uint8>::max();
constexpr glm::uint8 MIN_UINT8 = std::numeric_limits<glm::uint8>::min();

ArgParser &arg_parser = ArgParser::getInstance();

int main(int argc, const char *argv[]) {
    arg_parser.parseCommandLine(argc, argv);

    Mesh mesh = parsePlyFile(arg_parser.input_filename);

    DistanceFieldVolumeData volume_data;
    generateDistanceFieldVolumeData(mesh, mesh.getAABB(), arg_parser.DF_resolution_scale, volume_data);

    /// visualization for mip0

    fmt::memory_buffer buffer;
    glm::uint vertex_count = 0;

    Box const &mesh_bounds = volume_data.localSpaceMeshBounds;
    SparseDistanceFieldMip const &mip0 = volume_data.mips[0];

    const glm::uint indirection_table_size = mip0.indirectionDimensions.x * mip0.indirectionDimensions.y * mip0.indirectionDimensions.z;
    const glm::uint indirection_table_size_bytes = indirection_table_size * sizeof(glm::uint32);
    const glm::uint brick_size = DistanceField::BrickSize * DistanceField::BrickSize * DistanceField::BrickSize;
    const glm::uint brick_size_bytes = brick_size * sizeof(glm::uint8);
    const glm::uvec3 dimensions = mip0.indirectionDimensions;

    auto *indirection_table = reinterpret_cast<glm::uint32 *>(volume_data.alwaysLoadedMip.data());
    glm::uint8 *brick_data = volume_data.alwaysLoadedMip.data() + indirection_table_size * sizeof(glm::uint32);

    assert(volume_data.alwaysLoadedMip.size() == indirection_table_size_bytes + brick_size_bytes * mip0.numDistanceFieldBricks);
    glm::uint sample_count = 0;

    const glm::vec3 distance_field_voxel_size = mesh_bounds.getSize() / glm::vec3(dimensions * DistanceField::UniqueDataBrickSize -
                                                                                  2 * DistanceField::MeshDistanceFieldObjectBorder);
    const Box distance_field_volume_bounds = mesh_bounds.expandBy(distance_field_voxel_size);
    const glm::vec3 indirection_voxel_size = distance_field_voxel_size * (float) DistanceField::UniqueDataBrickSize;

    for (glm::uint position_index = 0; position_index < indirection_table_size; ++position_index) {
        const glm::uint32 brick_offset = indirection_table[position_index];

        glm::vec3 display_color;
        if (brick_offset == DistanceField::InvalidBrickIndex) {
            display_color = glm::vec3(0, 0, 0);
        } else {
            display_color = glm::vec3(200, 200, 200);
        }

        const glm::uvec3 brick_coordinate{
            position_index % dimensions.x,
            position_index / dimensions.x % dimensions.y,
            position_index / dimensions.x / dimensions.y % dimensions.z,
        };

        const glm::vec3 brick_min_position = distance_field_volume_bounds.min + glm::vec3(brick_coordinate) * indirection_voxel_size;
        for (glm::uint i = 0; i < brick_size; ++i) {
            const glm::uvec3 voxel_coordinate = {
                i % DistanceField::BrickSize,
                i / DistanceField::BrickSize % DistanceField::BrickSize,
                i / DistanceField::BrickSize / DistanceField::BrickSize,
            };

            const glm::vec3 sample_position = glm::vec3(voxel_coordinate) * distance_field_voxel_size + brick_min_position;

            fmt::format_to(std::back_inserter(buffer), "{} {}\n", sample_position, display_color);
        }
        vertex_count += brick_size;
    }

    // for (glm::uint z_index = 0; z_index < dimensions.z; ++z_index) {
    //     for (glm::uint y_index = 0; y_index < dimensions.y; ++y_index) {
    //         for (glm::uint x_index = 0; x_index < dimensions.x; ++x_index) {
    //             const glm::vec3 query_position = mesh_bound.min + voxel_size * glm::vec3{x_index, y_index, z_index};
    //             const embree::ClosestQueryResult query_result = distance_query.query(query_position, trace_distance);

    //             if (!query_result.valid) {
    //                 continue; // skip invalid query
    //             }

    //             const glm::vec3 closest_position = query_result.closestPoint;

    //             if (std::fabs(glm::length(query_position - closest_position) - display_distance) > band_size) {
    //                 continue; // skip far away points
    //             }

    //             bool inside_mesh = false;

    //             if (!(sample_mode && !outside_only)) {
    //                 int hit_back_count = 0;
    //                 for (const glm::vec3 unit_ray_direction : sample_directions) {
    //                     const float pullback_epsilon = 1e-4f;
    //                     const glm::vec3 start_pos = query_position - pullback_epsilon * trace_distance * unit_ray_direction;

    //                     // TODO: test ray intersect with bounding first
    //                     embree::RayHit rayhit = intersect.emitRay(start_pos, unit_ray_direction, trace_distance);
    //                     if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID && rayhit.hit.primID != RTC_INVALID_GEOMETRY_ID) {
    //                         const glm::vec3 hit_normal = rayhit.getHitNormal();
    //                         if (glm::dot(unit_ray_direction, hit_normal) > 0) {
    //                             hit_back_count++;
    //                         }
    //                     }
    //                 }

    //                 if (hit_back_count && hit_back_count > sample_directions.size() / 4) {
    //                     // consider it inside if significant ray hit back
    //                     inside_mesh = true;
    //                 }
    //             }

    //             if (outside_only && inside_mesh) continue; // skip points inside mesh

    //             if (sample_mode) {
    //                 fmt::format_to(std::back_inserter(buffer), "{} {} {} {} {} {}\n", query_position.x, query_position.y,
    //                 query_position.z,
    //                                255, 0, 0);
    //                 fmt::format_to(std::back_inserter(buffer), "{} {} {} {} {} {}\n", closest_position.x, closest_position.y,
    //                                closest_position.z, 0, 0, 255);
    //                 vertex_count += 2;
    //             } else {
    //                 auto gray_scale =
    //                     (glm::uint8) glm::clamp(std::round((1.0f - query_result.getDistance() / band_size) * 255.0f), 0.0f, 255.0f);

    //                 // shade sample points inside as blue, outside as gray
    //                 fmt::format_to(std::back_inserter(buffer), "{} {} {} {} {} {}\n", query_position.x, query_position.y,
    //                 query_position.z,
    //                                gray_scale, gray_scale, inside_mesh ? 255 : gray_scale);
    //                 vertex_count++;
    //             }
    //         }
    //     }
    // }

    // ------------------------------ write PLY file --------------------------------
    FILE *output_file = nullptr;
    fopen_s(&output_file, arg_parser.output_filename, "w+");

    // write ply header
    fmt::print(output_file, "ply\nformat ascii 1.0\n");
    fmt::print(output_file, "element vertex {}\n", vertex_count);
    fmt::print(output_file, "property float x\nproperty float y\nproperty float z\n");
    fmt::print(output_file, "property uchar red\nproperty uchar green\nproperty uchar blue\n");
    fmt::print(output_file, "end_header\n");
    // write vertices
    fmt::print(output_file, "{}", fmt::to_string(buffer));

    std::fclose(output_file);
}