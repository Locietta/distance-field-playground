#include "arg_parser.h"
#include "embree_wrapper.h"
#include "local_sdf.h"
#include "mesh.h"
#include "sdf_math.h"

#include "format.hpp"
#include <chrono>
#include <fmt/core.h>
#include <fstream>
#include <glm/common.hpp>

void write_ply_file(const char *filename, fmt::memory_buffer const &vertex_descs, glm::uint32 vertex_count);

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

    Box const &mesh_bounds = volume_data.local_space_mesh_bounds;

    fmt::memory_buffer buffer;
    fmt::memory_buffer buffer_invalid_brick; // for visualization of invalid brick

    for (glm::uint32 mip_index = 0; mip_index < DistanceField::NUM_MIPS; ++mip_index) {
        glm::uint32 vertex_count = 0;
        glm::uint32 vertex_count_invalid_brick = 0;

        SparseDistanceFieldMip const &mip = volume_data.mips[mip_index];

        const glm::uvec3 dimensions = mip.indirection_dimensions;
        const glm::uint32 indirection_table_size = dimensions.x * dimensions.y * dimensions.z;
        const glm::uint32 indirection_table_size_bytes = indirection_table_size * sizeof(glm::uint32);
        const glm::uint32 brick_size = DistanceField::BRICK_SIZE * DistanceField::BRICK_SIZE * DistanceField::BRICK_SIZE;
        const glm::uint32 brick_size_bytes = brick_size * sizeof(glm::uint8);

        const glm::uint32 *indirection_table = nullptr;
        const glm::uint8 *brick_data = nullptr;

        if (mip_index == DistanceField::NUM_MIPS - 1) {
            assert(volume_data.always_loaded_mip.size() == indirection_table_size_bytes + brick_size_bytes * mip.num_distance_field_bricks);
            indirection_table = reinterpret_cast<const glm::uint32 *>(volume_data.always_loaded_mip.data());
            brick_data = volume_data.always_loaded_mip.data() + indirection_table_size_bytes;
        } else {
            assert(mip.bulk_size == indirection_table_size_bytes + brick_size_bytes * mip.num_distance_field_bricks);
            indirection_table = reinterpret_cast<const glm::uint32 *>(volume_data.streamable_mips.data() + mip.bulk_offset);
            brick_data = volume_data.streamable_mips.data() + mip.bulk_offset + indirection_table_size_bytes;
        }

        assert(indirection_table && brick_data);

        glm::uint32 sample_count = 0;

        const glm::vec3 distance_field_voxel_size = mesh_bounds.getSize() / glm::vec3(dimensions * DistanceField::UNIQUE_DATA_BRICK_SIZE -
                                                                                      2 * DistanceField::MESH_DISTANCE_FIELD_OBJECT_BORDER);
        const Box distance_field_volume_bounds = mesh_bounds.expandBy(distance_field_voxel_size);
        const glm::vec3 indirection_voxel_size = distance_field_voxel_size * (float) DistanceField::UNIQUE_DATA_BRICK_SIZE;

        for (glm::uint32 position_index = 0; position_index < indirection_table_size; ++position_index) {
            const glm::uint32 brick_offset = indirection_table[position_index];

            bool is_valid_brick = brick_offset != DistanceField::INVALID_BRICK_INDEX;
            glm::vec3 display_color = is_valid_brick ? glm::vec3(200, 200, 200) : glm::vec3(0, 0, 0);
            fmt::memory_buffer &visual_buffer = is_valid_brick ? buffer : buffer_invalid_brick;

            const glm::uvec3 brick_coordinate{
                position_index % dimensions.x,
                position_index / dimensions.x % dimensions.y,
                position_index / dimensions.x / dimensions.y % dimensions.z,
            };

            const glm::vec3 brick_min_position = distance_field_volume_bounds.min + glm::vec3(brick_coordinate) * indirection_voxel_size;
            for (glm::uint32 i = 0; i < brick_size; ++i) {
                const glm::uvec3 voxel_coordinate = {
                    i % DistanceField::BRICK_SIZE,
                    i / DistanceField::BRICK_SIZE % DistanceField::BRICK_SIZE,
                    i / DistanceField::BRICK_SIZE / DistanceField::BRICK_SIZE,
                };

                const glm::vec3 sample_position = glm::vec3(voxel_coordinate) * distance_field_voxel_size + brick_min_position;

                fmt::format_to(std::back_inserter(visual_buffer), "{} {}\n", sample_position, display_color);
            }

            if (is_valid_brick) {
                vertex_count += brick_size;
            } else {
                vertex_count_invalid_brick += brick_size;
            }
        }

        write_ply_file(fmt::format("{}{}_valid_bricks.ply", arg_parser.output_filename, mip_index).c_str(), buffer, vertex_count);
        write_ply_file(fmt::format("{}{}_invalid_bricks.ply", arg_parser.output_filename, mip_index).c_str(), buffer_invalid_brick,
                       vertex_count_invalid_brick);

        buffer.clear();
        buffer_invalid_brick.clear();
    }

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

void write_ply_file(const char *filename, fmt::memory_buffer const &vertex_descs, glm::uint32 vertex_count) {
    FILE *output_file = fopen(filename, "w+");
    if (output_file == nullptr) fmt::print(stderr, "Failed to open {}\n", filename);

    // write ply header
    fmt::print(output_file, "ply\nformat ascii 1.0\n");
    fmt::print(output_file, "element vertex {}\n", vertex_count);
    fmt::print(output_file, "property float x\nproperty float y\nproperty float z\n");
    fmt::print(output_file, "property uchar red\nproperty uchar green\nproperty uchar blue\n");
    fmt::print(output_file, "end_header\n");
    // write vertices
    fmt::print(output_file, "{}", fmt::to_string(vertex_descs));
    std::fclose(output_file);
}