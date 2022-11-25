#include "arg_parser.h"
#include "embree_wrapper.h"
#include "local_sdf.h"
#include "mesh.h"
#include "sdf_dump.h"
#include "sdf_math.h"

#include "format.hpp"
#include <chrono>
#include <fmt/core.h>
#include <fstream>
#include <glm/common.hpp>

static ArgParser &arg_parser = ArgParser::getInstance();

int main(int argc, const char *argv[]) {
    arg_parser.parseCommandLine(argc, argv);

    auto read_start_time = std::chrono::system_clock::now();
    const std::vector<Mesh> meshes = Mesh::importFromFile(arg_parser.input_filename);
    const Mesh &mesh = meshes.front();
    auto read_end_time = std::chrono::system_clock::now();
    fmt::print("Read PLY model '{}' in {:.1f}s.\n", arg_parser.input_filename,
               std::chrono::duration<double>(read_end_time - read_start_time).count());

    DistanceFieldVolumeData volume_data;
    generate_distance_field_volume_data(mesh, mesh.getAABB(), arg_parser.df_resolution_scale, volume_data);

    /// visualization for mips

    auto write_start_time = std::chrono::system_clock::now();

    dump_sdf_volume_for_visualization(volume_data);

    auto write_end_time = std::chrono::system_clock::now();
    fmt::print("Write results in {:.1f}s.\n", std::chrono::duration<double>(write_end_time - write_start_time).count());

    auto serialize_start_time = std::chrono::steady_clock::now();

    // serialize to binary file
    std::ofstream fout{fmt::format("{}.bin", arg_parser.output_filename), std::ios_base::binary};
    DistanceFieldVolumeData::serialize(fout, volume_data);

    auto serialize_end_time = std::chrono::steady_clock::now();
    fmt::print("Write binary results in {:.1f}ms.\n",
               std::chrono::duration<double>(serialize_end_time - serialize_start_time).count() * 1000);

    // std::ifstream fin{fmt::format("{}.bin", arg_parser.output_filename), std::ios_base::binary};
    // DistanceFieldVolumeData tmp;
    // DistanceFieldVolumeData::deserialize(fin, tmp);
    return 0;
}