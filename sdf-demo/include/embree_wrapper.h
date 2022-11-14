#pragma once

#include <embree3/rtcore.h>
#include <embree3/rtcore_ray.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <vector>

class Mesh;

namespace embree {

struct Geometry {
    std::vector<glm::uvec3> indices_buffer;
    std::vector<glm::vec3> vertices_buffer;
    RTCGeometry internal = nullptr;
};

class Scene {
public:
    Scene();

    ~Scene();

    void addMesh(Mesh const &mesh);

    void commit();

    RTCDevice device_;
    RTCScene scene_;
    std::vector<Geometry> geos_;
};

class RayHit : public RTCRayHit {
public:
    RayHit(glm::vec3 const &origin, glm::vec3 const &direction, float far);

    [[nodiscard]] glm::vec3 getHitNormal() const;
    [[nodiscard]] bool isValidHit() const { return hit.geomID != RTC_INVALID_GEOMETRY_ID && hit.primID != RTC_INVALID_GEOMETRY_ID; }
};

class IntersectionContext : public RTCIntersectContext {
public:
    IntersectionContext(Scene const &scene) : scene_{scene.scene_} { rtcInitIntersectContext(this); }

    RayHit emitRay(glm::vec3 const &origin, glm::vec3 const &direction, float far);

    void emitRay(RayHit *rayhit) { rtcIntersect1(scene_, this, rayhit); }

private:
    RTCScene const &scene_;
};

class ClosestQueryResult {
public:
    glm::vec3 closest_point;

    // not free lunch, will call `sqrt()`
    [[nodiscard]] float getDistance() const;

private:
    friend class ClosestQueryContext;

    float query_distance_sq;
};

class ClosestQueryContext : public RTCPointQueryContext {
public:
    ClosestQueryContext(Scene const &scene);

    ClosestQueryResult query(glm::vec3 center, float radius);

    float queryDistance(glm::vec3 center, float radius) { return query(center, radius).getDistance(); }

private:
    static bool closestQueryFunc(RTCPointQueryFunctionArguments *args);

    RTCScene const &scene_;
    std::vector<RTCGeometry> mesh_geometries_;
    std::vector<glm::uint32> num_triangles_;
};

} // namespace embree
