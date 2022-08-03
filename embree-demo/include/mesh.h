#pragma once

#include <glm/vec3.hpp>
#include <vector>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec3 texCoord;
};

struct Box {
    glm::vec3 min;
    glm::vec3 max;

    [[nodiscard]] glm::vec3 getSize() const { return max - min; }
    [[nodiscard]] glm::vec3 getExtent() const { return (max - min) * 0.5f; }
    [[nodiscard]] glm::vec3 getCenter() const { return (max + min) * 0.5f; }

    [[nodiscard]] Box expandBy(glm::vec3 size) const { return {min - size, max + size}; }
};

struct Mesh {
    std::vector<glm::vec3> vertices;
    std::vector<glm::uvec3> indices;
    std::vector<glm::vec3> faceNormal;

    [[nodiscard]] Box getAABB() const;
    [[nodiscard]] Box getExpandedBoundingBox() const;
};
