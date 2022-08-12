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

constexpr int NumBufferVerts = 1; // Reserve extra space at the end of the array, as embree has an internal bug where
                                  // they read and discard 4 bytes off the end of the array

void Scene::addMesh(Mesh const &mesh) {
    geos_.push_back({mesh.indices, mesh.vertices, nullptr});

    Geometry &curr_geo = geos_.back();

    curr_geo.internal = rtcNewGeometry(device_, RTC_GEOMETRY_TYPE_TRIANGLE);
    rtcSetSharedGeometryBuffer(curr_geo.internal, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, curr_geo.verticesBuffer.data(), 0,
                               sizeof(glm::vec3), curr_geo.verticesBuffer.size());
    rtcSetSharedGeometryBuffer(curr_geo.internal, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, curr_geo.indicesBuffer.data(), 0,
                               sizeof(glm::uvec3), curr_geo.indicesBuffer.size());
}

void Scene::commit() {
    for (auto &geo_ : geos_) {
        rtcSetGeometryUserData(geo_.internal, &geo_);

        rtcCommitGeometry(geo_.internal);
        rtcAttachGeometry(scene_, geo_.internal);
        rtcReleaseGeometry(geo_.internal); // ??
    }
    rtcCommitScene(scene_);
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
    for (const auto &geo_ : scene.geos_) {
        mesh_geometries_.push_back(geo_.internal);
        num_triangles_.push_back(geo_.indicesBuffer.size());
    }
}

float ClosestQueryResult::getDistance() const {
    return std::sqrt(queryDistanceSq);
}

bool ClosestQueryContext::ClosestQueryFunc(RTCPointQueryFunctionArguments *args) {
    const auto *Context = reinterpret_cast<const ClosestQueryContext *>(args->context);

    assert(args->userPtr);
    ClosestQueryResult &ClosestQuery = *reinterpret_cast<ClosestQueryResult *>(args->userPtr);

    const std::uint32_t MeshIndex = args->geomID;
    const std::uint32_t TriangleIndex = args->primID;

    assert(TriangleIndex < Context->num_triangles_[MeshIndex]);

    const auto *VertexBuffer = (const glm::vec3 *) rtcGetGeometryBufferData(Context->mesh_geometries_[MeshIndex], RTC_BUFFER_TYPE_VERTEX, 0);
    const auto *IndexBuffer = (const std::uint32_t *) rtcGetGeometryBufferData(Context->mesh_geometries_[MeshIndex], RTC_BUFFER_TYPE_INDEX, 0);

    const std::uint32_t I0 = IndexBuffer[TriangleIndex * 3 + 0];
    const std::uint32_t I1 = IndexBuffer[TriangleIndex * 3 + 1];
    const std::uint32_t I2 = IndexBuffer[TriangleIndex * 3 + 2];

    const glm::vec3 V0 = VertexBuffer[I0];
    const glm::vec3 V1 = VertexBuffer[I1];
    const glm::vec3 V2 = VertexBuffer[I2];

    const glm::vec3 QueryPosition(args->query->x, args->query->y, args->query->z);

    const glm::vec3 ClosestPoint = closestPointOnTriangle(QueryPosition, V0, V1, V2);
    const float QueryDistanceSq = glm::dot(ClosestPoint - QueryPosition, ClosestPoint - QueryPosition);

    if (QueryDistanceSq < ClosestQuery.queryDistanceSq) {
        ClosestQuery.queryDistanceSq = QueryDistanceSq;
        ClosestQuery.closestPoint = ClosestPoint;

        // ClosestQuery.valid = true;
        bool bShrinkQuery = true;

        if (bShrinkQuery) {
            args->query->radius = std::sqrt(QueryDistanceSq);
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
    closest_query.queryDistanceSq = radius * radius;

    rtcPointQuery(scene_, &point_query, this, ClosestQueryFunc, &closest_query);

    return closest_query;
}