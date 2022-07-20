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
};

struct Mesh {
    std::vector<glm::vec3> vertices;
    std::vector<glm::uvec3> indices;

    [[nodiscard]] Box getExpandedBoundingBox() const;
};

