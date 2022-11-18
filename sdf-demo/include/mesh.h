#pragma once

#include <glm/vec3.hpp>
#include <vector>

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

    [[nodiscard]] Box getAABB() const;
    [[nodiscard]] Box getExpandedBoundingBox() const;

    [[nodiscard]] Mesh translate(glm::vec3 displacement) const;

    static std::vector<Mesh> importFromFile(const char *file_path);
};
