#include "embree_wrapper.h"
#include "geometry_math.h"
#include "mesh.h"
#include "ply_loader.h"

#include <fmt/core.h>
#include <fmt/format.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#define next_and_check(i)                                                                                              \
    (i)++;                                                                                                             \
    assert((i) < argc)

int main(int argc, const char *argv[]) {
    const char *input_filename = "meshes/test_sphere.ply";
    const char *output_filename = "DF_OUTPUT.ply";
    float voxel_size = 0.05;
    float band_size = 0.1f;
    float display_distance = 0.0f;
    bool outside_only = false;

    for (int i = 0; i < argc; ++i) {
        if (!strcmp(argv[i], "-i")) {
            next_and_check(i);
            input_filename = argv[i];
        } else if (!strcmp(argv[i], "-o")) {
            next_and_check(i);
            output_filename = argv[i];
        } else if (!strcmp(argv[i], "-v")) {
            next_and_check(i);
            voxel_size = (float) atof(argv[i]);
        } else if (!strcmp(argv[i], "-band")) {
            next_and_check(i);
            band_size = (float) atof(argv[i]);
        } else if (!strcmp(argv[i], "-d")) {
            next_and_check(i);
            display_distance = (float) atof(argv[i]);
        } else if (!strcmp(argv[i], "-outside")) {
            outside_only = true;
        }
    }

    Mesh mesh = parsePlyFile(input_filename);

    Box mesh_bound = mesh.getExpandedBoundingBox();
    glm::vec3 bounding_size = mesh_bound.getSize();

    glm::uvec3 dimensions = glm::max(glm::round(bounding_size / voxel_size), 1.0f);
    glm::vec3 actual_voxel_size = bounding_size / (glm::vec3) dimensions;

    embree::Scene embree_scene;
    embree_scene.setVertices(std::move(mesh.vertices));
    embree_scene.setIndices(std::move(mesh.indices));
    embree_scene.commit();

    // prepare for tracing consts
    std::vector<glm::vec3> sample_directions;
    {
        const int num_voxel_distance_samples = 49;
        sample_directions = stratifiedUniformHemisphereSamples(num_voxel_distance_samples);
        std::vector<glm::vec3> other_half_samples = stratifiedUniformHemisphereSamples(num_voxel_distance_samples);
        for (const auto &other_half_sample : other_half_samples) {
            sample_directions.emplace_back(other_half_sample.x, other_half_sample.y, -other_half_sample.z);
        }
    }
    const float trace_distance = band_size * 2;

    embree::IntersectionContext intersect{embree_scene};
    embree::ClosestQueryContext distance_query{embree_scene};

    fmt::memory_buffer buffer;
    glm::uint vertex_count = 0;

    for (glm::uint z_index = 0; z_index < dimensions.z; ++z_index) {
        for (glm::uint y_index = 0; y_index < dimensions.y; ++y_index) {
            for (glm::uint x_index = 0; x_index < dimensions.x; ++x_index) {
                const glm::vec3 query_position =
                    mesh_bound.min + actual_voxel_size * glm::vec3{x_index, y_index, z_index};
                const embree::ClosestQueryResult query_result = distance_query.query(query_position, trace_distance);

                if (!query_result.valid) {
                    continue; // skip invalid query
                }

                const glm::vec3 closest_position = query_result.closestPoint;

                if (std::fabs(glm::length(query_position - closest_position) - display_distance) > band_size) {
                    continue; // skip far away points
                }

                if (outside_only) {
                    int hit_back_count = 0;
                    for (const glm::vec3 unit_ray_direction : sample_directions) {
                        const float pullback_epsilon = 1e-4f;
                        const glm::vec3 start_pos =
                            query_position - pullback_epsilon * trace_distance * unit_ray_direction;

                        // TODO: test ray intersect with bounding first
                        embree::RayHit rayhit = intersect.emitRay(start_pos, unit_ray_direction, trace_distance);
                        if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID &&
                            rayhit.hit.primID != RTC_INVALID_GEOMETRY_ID) {
                            const glm::vec3 hit_normal = rayhit.getHitNormal();
                            if (glm::dot(unit_ray_direction, hit_normal) > 0) {
                                hit_back_count++;
                            }
                        }
                    }

                    if (hit_back_count && hit_back_count > sample_directions.size() / 4) {
                        // consider it inside if significant ray hit back
                        continue; // skip inside points
                    }
                }

                // auto gray_scale = (glm::uint8) glm::clamp(
                //     std::round((1.0f - distance / (glm::length(bounding_size) / band_size_scale)) * 255.0f), 0.0f,
                //     255.0f);
                fmt::format_to(std::back_inserter(buffer), "{} {} {} {} {} {}\n", query_position.x, query_position.y,
                               query_position.z, 255, 0, 0);
                fmt::format_to(std::back_inserter(buffer), "{} {} {} {} {} {}\n", closest_position.x,
                               closest_position.y, closest_position.z, 0, 0, 255);
                vertex_count += 2;
            }
        }
    }

    // ------------------------------ write PLY file --------------------------------
    FILE *output_file = nullptr;
    fopen_s(&output_file, output_filename, "w+");

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