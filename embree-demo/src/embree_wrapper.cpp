#include "embree_wrapper.h"
#include "geometry_math.h"
#include <fmt/core.h>
#include <glm/geometric.hpp>
#include <iterator>

using namespace embree;

Scene::Scene() {
    device_ = rtcNewDevice(nullptr);
    // TODO: error handling
    scene_ = rtcNewScene(device_);
    rtcSetSceneFlags(scene_, RTC_SCENE_FLAG_NONE);
    geo_.internal = rtcNewGeometry(device_, RTC_GEOMETRY_TYPE_TRIANGLE);
}

Scene::~Scene() {
    rtcReleaseScene(scene_);
    rtcReleaseDevice(device_);
    if (geo_.internal != nullptr) {
        rtcReleaseGeometry(geo_.internal);
    }
}

constexpr int NumBufferVerts = 1; // Reserve extra space at the end of the array, as embree has an internal bug where
                                  // they read and discard 4 bytes off the end of the array

void Scene::setVertices(std::vector<glm::vec3> &&vertices) {
    geo_.verticesBuffer = std::move(vertices);
    rtcSetSharedGeometryBuffer(geo_.internal, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, geo_.verticesBuffer.data(),
                               0, sizeof(glm::vec3), geo_.verticesBuffer.size());
}

void Scene::setVertices(const glm::vec3 *vertices, std::size_t buffer_size) {
    geo_.verticesBuffer.clear();
    geo_.verticesBuffer.reserve(buffer_size);
    std::copy(vertices, vertices + buffer_size, std::back_inserter(geo_.verticesBuffer));

    rtcSetSharedGeometryBuffer(geo_.internal, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, geo_.verticesBuffer.data(),
                               0, sizeof(glm::vec3), geo_.verticesBuffer.size());
}

void Scene::setIndices(std::vector<glm::uvec3> &&indices) {
    geo_.indicesBuffer = std::move(indices);
    rtcSetSharedGeometryBuffer(geo_.internal, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, geo_.indicesBuffer.data(), 0,
                               sizeof(glm::uvec3), geo_.indicesBuffer.size());
}

void Scene::setIndices(const glm::uvec3 *indices, std::size_t buffer_size) {
    geo_.indicesBuffer.clear();
    geo_.indicesBuffer.reserve(buffer_size);
    std::copy(indices, indices + buffer_size, std::back_inserter(geo_.indicesBuffer));

    rtcSetSharedGeometryBuffer(geo_.internal, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, geo_.indicesBuffer.data(), 0,
                               sizeof(glm::uvec3), geo_.indicesBuffer.size());
}

void Scene::commit() {
    rtcSetGeometryUserData(geo_.internal, &geo_);

    rtcCommitGeometry(geo_.internal);
    rtcAttachGeometry(scene_, geo_.internal);
    rtcCommitScene(scene_);
}

RayHit IntersectionContext::emitRay(glm::vec3 const &origin, glm::vec3 const &direction, glm::vec2 const &near_far) {
    RayHit rayhit{origin, direction, near_far};
    rtcIntersect1(scene_, this, &rayhit);
    return rayhit;
}

RayHit::RayHit(glm::vec3 const &origin, glm::vec3 const &direction, glm::vec2 const &near_far) {
    hit.u = hit.v = 0;
    ray.time = 0;
    ray.mask = 0xFFFFFFFF;

    ray.tnear = near_far[0];
    ray.tfar = near_far[1];

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

bool DistanceQueryContext::DistanceQueryFunc(RTCPointQueryFunctionArguments *args) {
    const auto *Context = reinterpret_cast<const DistanceQueryContext *>(args->context);

    assert(args->userPtr);
    float &ClosestDistanceSq = *reinterpret_cast<float *>(args->userPtr);

    const std::uint32_t TriangleIndex = args->primID;
    assert(TriangleIndex < Context->num_triangles_);

    const auto *VertexBuffer =
        (const glm::vec3 *) rtcGetGeometryBufferData(Context->mesh_geometry_, RTC_BUFFER_TYPE_VERTEX, 0);
    const auto *IndexBuffer =
        (const std::uint32_t *) rtcGetGeometryBufferData(Context->mesh_geometry_, RTC_BUFFER_TYPE_INDEX, 0);

    const std::uint32_t I0 = IndexBuffer[TriangleIndex * 3 + 0];
    const std::uint32_t I1 = IndexBuffer[TriangleIndex * 3 + 1];
    const std::uint32_t I2 = IndexBuffer[TriangleIndex * 3 + 2];

    const glm::vec3 V0 = VertexBuffer[I0];
    const glm::vec3 V1 = VertexBuffer[I1];
    const glm::vec3 V2 = VertexBuffer[I2];

    const glm::vec3 QueryPosition(args->query->x, args->query->y, args->query->z);

    const glm::vec3 ClosestPoint = closestPointOnTriangle(QueryPosition, V0, V1, V2);
    const float QueryDistanceSq = glm::dot(ClosestPoint - QueryPosition, ClosestPoint - QueryPosition);

    if (QueryDistanceSq < ClosestDistanceSq) {
        ClosestDistanceSq = QueryDistanceSq;

        bool bShrinkQuery = true;

        if (bShrinkQuery) {
            args->query->radius = std::sqrt(ClosestDistanceSq);
            // Return true to indicate that the query radius has shrunk
            return true;
        }
    }

    // Return false to indicate that the query radius hasn't changed
    return false;
}

float DistanceQueryContext::queryDistance(glm::vec3 center, float radius) {
    PointQuery point_query{center, radius};
    float queryDistanceSq = (2.0f * radius) * (2.0f * radius);

    rtcPointQuery(scene_, &point_query, this, DistanceQueryFunc, &queryDistanceSq);

    return std::sqrt(queryDistanceSq);
}