#include "mesh.h"
#include <execution>
#include <glm/common.hpp>


Box Mesh::getExpandedBoundingBox() const {
    glm::vec3 min{std::numeric_limits<float>::max()};
    glm::vec3 max{-std::numeric_limits<float>::max()};

    for (const auto &vertex : vertices) {
        min = glm::min(min, vertex);
        max = glm::max(max, vertex);
    }

    return {
        .min = min + (min - max) / 4.0f,
        .max = max + (max - min) / 4.0f,
    };
}

Box Mesh::getAABB() const {
    glm::vec3 min{std::numeric_limits<float>::max()};
    glm::vec3 max{-std::numeric_limits<float>::max()};

    for (const auto &vertex : vertices) {
        min = glm::min(min, vertex);
        max = glm::max(max, vertex);
    }

    return {min, max};
}

Mesh Mesh::translate(glm::vec3 displacement) const {
    std::vector<glm::vec3> out_verts;
    std::transform(std::execution::par_unseq, vertices.cbegin(), vertices.cend(), std::back_inserter(out_verts),
                   [displacement](glm::vec3 const &v) { return v + displacement; });
    return Mesh{out_verts, indices};
}