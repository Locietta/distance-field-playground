#pragma once

#include <algorithm>
#include <embree3/rtcore.h>
#include <embree3/rtcore_ray.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <utility>
#include <vector>

namespace embree {

struct Geometry {
    std::vector<glm::uvec3> indicesBuffer;
    std::vector<glm::vec3> verticesBuffer;
    RTCGeometry internal = nullptr;
};

class Scene {
public:
    Scene();

    ~Scene();

    void setVertices(std::vector<glm::vec3> &&vertices);

    void setVertices(const glm::vec3 *vertices, std::size_t buffer_size);

    template <std::size_t N>
    void setVertices(const glm::vec3 (&vertices)[N]) {
        setVertices(vertices, N);
    }

    void setIndices(std::vector<glm::uvec3> &&indices);

    void setIndices(const glm::uvec3 *indices, std::size_t buffer_size);

    template <std::size_t N>
    void setIndices(const glm::uvec3 (&indices)[N]) {
        setIndices(indices, N);
    }

    void commit();

    RTCDevice device_;
    RTCScene scene_;
    Geometry geo_;
};

class RayHit : public RTCRayHit {
public:
    RayHit(glm::vec3 const &origin, glm::vec3 const &direction,
           glm::vec2 const &near_far = {0, std::numeric_limits<float>::infinity()});
    
    [[nodiscard]] glm::vec3 getHitNormal() const;
};

class IntersectionContext : public RTCIntersectContext {
public:
    IntersectionContext(Scene const &scene) : scene_{scene.scene_} { rtcInitIntersectContext(this); }

    RayHit emitRay(glm::vec3 const &origin, glm::vec3 const &direction,
                   glm::vec2 const &near_far = {0, std::numeric_limits<float>::max()});

    void emitRay(RayHit *rayhit) { rtcIntersect1(scene_, this, rayhit); }

private:
    RTCScene const &scene_;
};

class DistanceQueryContext : public RTCPointQueryContext {
public:
    DistanceQueryContext(Scene const &scene);

    float queryDistance(glm::vec3 center, float radius);
    glm::vec3 queryClosest(glm::vec3 center, float radius);

private:
    static bool DistanceQueryFunc(RTCPointQueryFunctionArguments *args);
    static bool ClosestQueryFunc(RTCPointQueryFunctionArguments *args);

    RTCScene const &scene_;
    RTCGeometry mesh_geometry_;
    glm::uint32 num_triangles_;
};

} // namespace embree
