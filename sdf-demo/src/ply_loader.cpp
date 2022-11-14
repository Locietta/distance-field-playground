#include "ply_loader.h"
#include "mesh.h"
#include <fmt/core.h>
#include <fstream>
#include <glm/geometric.hpp>
#include <sstream>
#include <string>

Mesh parse_ply_file(const char *filename) {
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

    for (int i = 0; i < vertex_number; ++i) {
        glm::vec3 vert;
        fin >> vert.x >> vert.y >> vert.z;

        fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // skip line

        mesh.vertices.emplace_back(vert);
    }

    for (int i = 0; i < face_number; ++i) {
        glm::uvec3 face;
        glm::uint32 vert_count;
        fin >> vert_count >> face[0] >> face[1] >> face[2];
        if (vert_count == 4) {
            fin >> vert_count; // use unused vert_count as face[3]
            mesh.indices.emplace_back(face[2], vert_count, face[0]);
        }
        fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // skip line
        mesh.indices.emplace_back(face);
    }

    return mesh;
}
