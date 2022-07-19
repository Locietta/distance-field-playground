#include "embree_wrapper.h"
#include "mesh.h"
#include "ply_loader.h"

#include <fmt/core.h>
#include <glm/vec3.hpp>

int main() {
    embree::Scene embree_scene;

    Mesh mesh = parsePlyFile("meshes/test_cube.ply");

    // TODO: bounding box, split into even bricks

    embree_scene.setVertices(std::move(mesh.vertices));
    embree_scene.setIndices(std::move(mesh.indices));

    embree_scene.commit();

    embree::IntersectionContext intersect{embree_scene};

    embree::RayHit rayhit = intersect.emitRay({0.25, 0.25, -1}, {0, 0, 1});

    if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
        fmt::print("[Ray intersection] Geo_ID: {}, distance: {:.2f}\n", rayhit.hit.geomID, rayhit.ray.tfar);
    } else {
        fmt::print("No ray intersection!\n");
    }

    embree::DistanceQueryContext distance_query{embree_scene};

    // sample

    glm::vec3 query_point = {3, 3, 3};

    float distance = distance_query.queryDistance(query_point, 5);

    fmt::print("Distance from ({}, {}, {}) and mesh is {}\n", query_point.x, query_point.y, query_point.z, distance);

    // convert distance to grayscale

    // write ply
}