#include "arg_parser.h"
#include "embree_wrapper.h"
#include "geometry_math.h"
#include "local_DF_utility.h"
#include "mesh.h"
#include "ply_loader.h"

#include "format.hpp"
#include <fmt/core.h>
#include <glm/common.hpp>

#define next_and_check(i)                                                                                                                  \
    (i)++;                                                                                                                                 \
    assert((i) < argc)

constexpr glm::uint8 MAX_UINT8 = std::numeric_limits<glm::uint8>::max();
constexpr glm::uint8 MIN_UINT8 = std::numeric_limits<glm::uint8>::min();

void writePlyFile(const char *filename, fmt::memory_buffer const &vertexDescs, glm::uint vertexCount);

ArgParser &arg_parser = ArgParser::getInstance();

int main(int argc, const char *argv[]) {
    arg_parser.parseCommandLine(argc, argv);

    Mesh mesh = parsePlyFile(arg_parser.input_filename);

    DistanceFieldVolumeData volume_data;
    generateDistanceFieldVolumeData(mesh, mesh.getAABB(), arg_parser.DF_resolution_scale, volume_data);

    /// visualization for mips

    Box const &mesh_bounds = volume_data.localSpaceMeshBounds;

    fmt::memory_buffer buffer;
    fmt::memory_buffer buffer_invalid_brick; // for visualization of invalid brick

    for (int mip_index = 0; mip_index < DistanceField::NumMips; ++mip_index) {
        glm::uint vertex_count = 0;
        glm::uint vertex_count_invalid_brick = 0;

        SparseDistanceFieldMip const &mip = volume_data.mips[mip_index];

        const glm::uint indirection_table_size = mip.indirectionDimensions.x * mip.indirectionDimensions.y * mip.indirectionDimensions.z;
        const glm::uint indirection_table_size_bytes = indirection_table_size * sizeof(glm::uint32);
        const glm::uint brick_size = DistanceField::BrickSize * DistanceField::BrickSize * DistanceField::BrickSize;
        const glm::uint brick_size_bytes = brick_size * sizeof(glm::uint8);
        const glm::uvec3 dimensions = mip.indirectionDimensions;

        glm::uint32 *indirection_table = nullptr;
        glm::uint8 *brick_data = nullptr;

        if (mip_index == DistanceField::NumMips - 1) {
            assert(volume_data.alwaysLoadedMip.size() == indirection_table_size_bytes + brick_size_bytes * mip.numDistanceFieldBricks);
            indirection_table = reinterpret_cast<glm::uint32 *>(volume_data.alwaysLoadedMip.data());
            brick_data = volume_data.alwaysLoadedMip.data() + indirection_table_size_bytes;
        } else {
            assert(mip.bulkSize == indirection_table_size_bytes + brick_size_bytes * mip.numDistanceFieldBricks);
            indirection_table = reinterpret_cast<glm::uint32 *>(volume_data.streamableMips.data() + mip.bulkOffset);
            brick_data = volume_data.streamableMips.data() + mip.bulkOffset + indirection_table_size_bytes;
        }

        assert(indirection_table && brick_data);

        glm::uint sample_count = 0;

        const glm::vec3 distance_field_voxel_size = mesh_bounds.getSize() / glm::vec3(dimensions * DistanceField::UniqueDataBrickSize -
                                                                                      2 * DistanceField::MeshDistanceFieldObjectBorder);
        const Box distance_field_volume_bounds = mesh_bounds.expandBy(distance_field_voxel_size);
        const glm::vec3 indirection_voxel_size = distance_field_voxel_size * (float) DistanceField::UniqueDataBrickSize;

        for (glm::uint position_index = 0; position_index < indirection_table_size; ++position_index) {
            const glm::uint32 brick_offset = indirection_table[position_index];

            bool is_valid_brick = brick_offset != DistanceField::InvalidBrickIndex;
            glm::vec3 display_color = is_valid_brick ? glm::vec3(200, 200, 200) : glm::vec3(0, 0, 0);
            fmt::memory_buffer &visual_buffer = is_valid_brick ? buffer : buffer_invalid_brick;

            const glm::uvec3 brick_coordinate{
                position_index % dimensions.x,
                position_index / dimensions.x % dimensions.y,
                position_index / dimensions.x / dimensions.y % dimensions.z,
            };

            const glm::vec3 brick_min_position = distance_field_volume_bounds.min + glm::vec3(brick_coordinate) * indirection_voxel_size;
            for (glm::uint i = 0; i < brick_size; ++i) {
                const glm::uvec3 voxel_coordinate = {
                    i % DistanceField::BrickSize,
                    i / DistanceField::BrickSize % DistanceField::BrickSize,
                    i / DistanceField::BrickSize / DistanceField::BrickSize,
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

        writePlyFile(fmt::format("{}{}_valid_bricks.ply", arg_parser.output_filename, mip_index).c_str(), buffer, vertex_count);
        writePlyFile(fmt::format("{}{}_invalid_bricks.ply", arg_parser.output_filename, mip_index).c_str(), buffer_invalid_brick,
                     vertex_count_invalid_brick);
        
        buffer.clear();
        buffer_invalid_brick.clear();
    }
}

void writePlyFile(const char *filename, fmt::memory_buffer const &vertex_descs, glm::uint vertex_count) {
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