#include "ply_loader.h"
#include "mesh.h"
#include <fmt/core.h>
#include <fstream>
#include <glm/geometric.hpp>
#include <sstream>
#include <string>

inline static glm::uvec3 regulateTriangleIndex(std::vector<glm::vec3> const &vertex_buffer, std::vector<glm::vec3> const &normal_buffer,
                                               glm::uvec3 triangle_index) {

    const glm::vec3 V0 = vertex_buffer[triangle_index[0]];
    const glm::vec3 V1 = vertex_buffer[triangle_index[1]];
    const glm::vec3 V2 = vertex_buffer[triangle_index[2]];

    const glm::vec3 N0 = normal_buffer[triangle_index[0]];
    const glm::vec3 N1 = normal_buffer[triangle_index[1]];
    const glm::vec3 N2 = normal_buffer[triangle_index[2]];

    const glm::vec3 face_normal = (N0 + N1 + N2) / 3.0f;
    if (glm::dot(glm::cross(V1 - V0, V2 - V0), face_normal) > 0.f) {
        return triangle_index;
    } else {
        return {triangle_index[0], triangle_index[2], triangle_index[1]};
    }
}

Mesh parsePlyFile(const char *filename) {
    std::ifstream fin(filename);

    if (!fin) {
        fmt::print(stderr, "Failed to open PLY file!\n");
        std::exit(1);
    }

    std::string line;
    std::getline(fin, line);

    if (line != "ply") { // sanity check
        fmt::print(stderr, "Invalid PLY file!\n");
        std::exit(1);
    }

    Mesh mesh;
    std::size_t vertex_number;
    std::size_t face_number;

    // parse header
    while (std::getline(fin, line)) {
        std::istringstream iss(line);
        std::string buf;
        iss >> buf;

        if (buf == "end_header") break;
        if (buf != "element") continue; // just ignore for simplicity

        // get element count
        iss >> buf;
        if (buf == "vertex") {
            iss >> vertex_number;
            mesh.vertices.reserve(vertex_number);
        } else if (buf == "face") {
            iss >> face_number;
            mesh.indices.reserve(face_number * 2); // for rect faces
        }
    }

    std::vector<glm::vec3> normal_buffer;
    normal_buffer.reserve(vertex_number);

    for (int i = 0; i < vertex_number; ++i) {
        glm::vec3 vert, normal;
        fin >> vert.x >> vert.y >> vert.z;
        fin >> normal.x >> normal.y >> normal.z;

        fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // skip line

        mesh.vertices.emplace_back(vert);
        normal_buffer.emplace_back(normal);
    }

    for (int i = 0; i < face_number; ++i) {
        glm::uvec3 face;
        glm::uint32 vert_count;
        fin >> vert_count >> face[0] >> face[1] >> face[2];
        if (vert_count == 4) {
            fin >> vert_count; // use unused vert_count as face[3]
            mesh.indices.emplace_back(regulateTriangleIndex(mesh.vertices, normal_buffer, {face[2], vert_count, face[0]}));
        }
        fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // skip line
        mesh.indices.emplace_back(regulateTriangleIndex(mesh.vertices, normal_buffer, face));
    }

    return mesh;
}
