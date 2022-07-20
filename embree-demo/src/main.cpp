#include "embree_wrapper.h"
#include "mesh.h"
#include "ply_loader.h"

#include <fmt/core.h>
#include <fmt/format.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

constexpr float voxel_size = 0.04;
constexpr const char *output_filename = "DF_OUTPUT.ply";

int main() {
    Mesh mesh = parsePlyFile("meshes/test_sphere.ply");

    Box mesh_bounding = mesh.getExpandedBoundingBox();
    glm::vec3 bounding_size = mesh_bounding.getSize();

    glm::uvec3 dimensions = glm::max(glm::round(bounding_size / voxel_size), 1.0f);
    glm::vec3 actual_voxel_size = bounding_size / (glm::vec3) dimensions;

    embree::Scene embree_scene;
    embree_scene.setVertices(std::move(mesh.vertices));
    embree_scene.setIndices(std::move(mesh.indices));
    embree_scene.commit();

    // embree::IntersectionContext intersect{embree_scene};
    // embree::RayHit rayhit = intersect.emitRay({0.25, 0.25, -1}, {0, 0, 1});

    // if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
    //     fmt::print("[Ray intersection] Geo_ID: {}, distance: {:.2f}\n", rayhit.hit.geomID,
    //     rayhit.ray.tfar);
    // } else {
    //     fmt::print("No ray intersection!\n");
    // }

    embree::DistanceQueryContext distance_query{embree_scene};

    fmt::memory_buffer buffer;
    glm::uint vertex_count = 0;

    for (glm::uint z_index = 0; z_index < dimensions.z; ++z_index) {
        for (glm::uint y_index = 0; y_index < dimensions.y; ++y_index) {
            for (glm::uint x_index = 0; x_index < dimensions.x; ++x_index) {
                glm::vec3 query_position = mesh_bounding.min + glm::vec3{x_index, y_index, z_index} * actual_voxel_size;
                float distance = distance_query.queryDistance(query_position, glm::length(bounding_size) * 2);

                if (distance > glm::length(bounding_size) / 12.0f) {
                    continue; // skip far away points
                }

                auto gray_scale = (glm::uint8) glm::clamp(
                    std::round((1.0f - distance / (glm::length(bounding_size) / 12.0f)) * 255.0f), 0.0f, 255.0f);
                fmt::format_to(std::back_inserter(buffer), "{} {} {} {} {} {}\n", query_position.x, query_position.y,
                               query_position.z, gray_scale, gray_scale, gray_scale);
                vertex_count++;
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