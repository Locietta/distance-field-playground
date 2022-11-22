#include "mesh.h"
#include <execution>
#include <filesystem>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h> // Post processing flags
#include <assimp/scene.h>       // Output data structure
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
    out_verts.reserve(vertices.size());

    std::for_each(std::execution::par_unseq, vertices.cbegin(), vertices.cend(),
                  [displacement, &out_verts](glm::vec3 v) { out_verts.push_back(v + displacement); });
    return Mesh{out_verts, indices};
}

std::vector<Mesh> Mesh::importFromFile(const char *file_path) {
    Assimp::Importer importer;
    const std::uint32_t import_flag = aiProcess_Triangulate | aiProcess_ImproveCacheLocality | aiProcess_RemoveComponent;

    const aiScene *scene = importer.ReadFile(file_path, import_flag);

    assert(scene->HasMeshes());

    std::vector<Mesh> result(scene->mNumMeshes);

    for (int i = 0; i < scene->mNumMeshes; ++i) {
        const auto &mesh = scene->mMeshes[i];

        result[i].vertices.resize(mesh->mNumVertices);
        result[i].indices.resize(mesh->mNumFaces);

        for (int j = 0; j < mesh->mNumVertices; ++j) {
            result[i].vertices[j] = {
                mesh->mVertices[j].x,
                mesh->mVertices[j].y,
                mesh->mVertices[j].z,
            };
        }

        for (int j = 0; j < mesh->mNumFaces; ++j) {
            auto const &face = mesh->mFaces[j];
            result[i].indices[j] = {
                face.mIndices[0],
                face.mIndices[1],
                face.mIndices[2],
            };
        }
    }

    return result;
}