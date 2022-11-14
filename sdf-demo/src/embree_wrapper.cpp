#include "embree_wrapper.h"
#include "geometry_math.h"
#include "mesh.h"
#include <fmt/core.h>
#include <glm/geometric.hpp>
#include <iterator>

using namespace embree;

Scene::Scene() {
    device_ = rtcNewDevice(nullptr);
    // TODO: error handling
    scene_ = rtcNewScene(device_);
    rtcSetSceneFlags(scene_, RTC_SCENE_FLAG_NONE);
}

Scene::~Scene() {
    rtcReleaseScene(scene_);
    rtcReleaseDevice(device_);
}

void Scene::addMesh(Mesh const &mesh) {
    geos_.push_back({mesh.indices, mesh.vertices, nullptr});

    Geometry &curr_geo = geos_.back();

    curr_geo.internal = rtcNewGeometry(device_, RTC_GEOMETRY_TYPE_TRIANGLE);
    rtcSetSharedGeometryBuffer(curr_geo.internal, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, curr_geo.vertices_buffer.data(), 0,
                               sizeof(glm::vec3), curr_geo.vertices_buffer.size());
    rtcSetSharedGeometryBuffer(curr_geo.internal, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, curr_geo.indices_buffer.data(), 0,
                               sizeof(glm::uvec3), curr_geo.indices_buffer.size());
}

void Scene::commit() {
    for (auto &geo : geos_) {
        rtcSetGeometryUserData(geo.internal, &geo);

        rtcCommitGeometry(geo.internal);
        rtcAttachGeometry(scene_, geo.internal);
        rtcReleaseGeometry(geo.internal);
    }
    rtcJoinCommitScene(scene_);
}

RayHit IntersectionContext::emitRay(glm::vec3 const &origin, glm::vec3 const &direction, float far) {
    RayHit rayhit{origin, direction, far};
    rtcIntersect1(scene_, this, &rayhit);
    return rayhit;
}

RayHit::RayHit(glm::vec3 const &origin, glm::vec3 const &direction, float far) {
    hit.u = hit.v = 0;
    ray.time = 0;
    ray.mask = 0xFFFFFFFF;

    ray.tnear = 0;
    ray.tfar = far;

    hit.geomID = RTC_INVALID_GEOMETRY_ID;
    hit.instID[0] = RTC_INVALID_GEOMETRY_ID;
    hit.primID = RTC_INVALID_GEOMETRY_ID;

    ray.org_x = origin.x;
    ray.org_y = origin.y;
    ray.org_z = origin.z;

    ray.dir_x = direction.x;
    ray.dir_y = direction.y;
    ray.dir_z = direction.z;
}

glm::vec3 RayHit::getHitNormal() const {
    const glm::vec3 hit_vec = {hit.Ng_x, hit.Ng_y, hit.Ng_z};

    const float unsafe_normal_epsilon = 1e-16f;
    if (glm::dot(hit_vec, hit_vec) < unsafe_normal_epsilon) {
        return {0, 0, 0};
    }

    return glm::normalize(hit_vec);
}

/// private class
class PointQuery : public RTCPointQuery {
public:
    PointQuery(glm::vec3 const &center, float r) {
        time = 0;
        x = center.x;
        y = center.y;
        z = center.z;
        radius = r;
    }
};

ClosestQueryContext::ClosestQueryContext(Scene const &scene) : scene_{scene.scene_} {
    rtcInitPointQueryContext(this);
    for (const auto &geo : scene.geos_) {
        mesh_geometries_.push_back(geo.internal);
        num_triangles_.push_back(geo.indices_buffer.size());
    }
}

float ClosestQueryResult::getDistance() const {
    return std::sqrt(query_distance_sq);
}

bool ClosestQueryContext::closestQueryFunc(RTCPointQueryFunctionArguments *args) {
    const auto *context = reinterpret_cast<const ClosestQueryContext *>(args->context);

    assert(args->userPtr);
    ClosestQueryResult &closest_query = *reinterpret_cast<ClosestQueryResult *>(args->userPtr);

    const std::uint32_t mesh_index = args->geomID;
    const std::uint32_t triangle_index = args->primID;

    assert(triangle_index < context->num_triangles_[mesh_index]);

    const auto *vertex_buffer =
        (const glm::vec3 *) rtcGetGeometryBufferData(context->mesh_geometries_[mesh_index], RTC_BUFFER_TYPE_VERTEX, 0);
    const auto *index_buffer =
        (const std::uint32_t *) rtcGetGeometryBufferData(context->mesh_geometries_[mesh_index], RTC_BUFFER_TYPE_INDEX, 0);

    const std::uint32_t I0 = index_buffer[triangle_index * 3 + 0];
    const std::uint32_t I1 = index_buffer[triangle_index * 3 + 1];
    const std::uint32_t I2 = index_buffer[triangle_index * 3 + 2];

    const glm::vec3 V0 = vertex_buffer[I0];
    const glm::vec3 V1 = vertex_buffer[I1];
    const glm::vec3 V2 = vertex_buffer[I2];

    const glm::vec3 query_position(args->query->x, args->query->y, args->query->z);

    const glm::vec3 closest_point = closest_point_on_triangle(query_position, V0, V1, V2);
    const float query_distance_sq = glm::dot(closest_point - query_position, closest_point - query_position);

    if (query_distance_sq < closest_query.query_distance_sq) {
        closest_query.query_distance_sq = query_distance_sq;
        closest_query.closest_point = closest_point;

        bool b_shrink_query = true;

        if (b_shrink_query) {
            args->query->radius = std::sqrt(query_distance_sq);
            // Return true to indicate that the query radius has shrunk
            return true;
        }
    }

    // Return false to indicate that the query radius hasn't changed
    return false;
}

ClosestQueryResult ClosestQueryContext::query(glm::vec3 center, float radius) {
    PointQuery point_query{center, radius};
    ClosestQueryResult closest_query;
    closest_query.query_distance_sq = radius * radius;

    rtcPointQuery(scene_, &point_query, this, closestQueryFunc, &closest_query);

    return closest_query;
}