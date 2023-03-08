#include "sdf_dump.h"

#include "arg_parser.h"
#include "format.hpp"
#include "local_sdf.h"
#include <execution>

namespace {

struct uchar4 { // NOLINT
    union {
        struct {
            glm::uint8 r, g, b, a;
        };
        struct {
            glm::uint8 x, y, z, w;
        };
        glm::uint8 data[4];
    };
};

struct Vertex {
    glm::vec3 position;
    uchar4 color;
};

class DistanceFieldDumpTask {
public:
    DistanceFieldDumpTask(const glm::uint32 *indirection_table, const glm::uint8 *brick_data, glm::uint32 position_index,
                          glm::uvec3 dimensions, glm::vec3 sdf_voxel_size, Box volume_bounds, bool calculate_color)
        : indirection_table{indirection_table}, brick_data{brick_data}, position_index{position_index}, dimensions{dimensions},
          sdf_voxel_size{sdf_voxel_size}, volume_bounds{volume_bounds}, calculate_color{calculate_color} {}

    void doWork() noexcept;

    // inputs, read-only
    const glm::uint32 *const indirection_table;
    const glm::uint8 *const brick_data;
    const glm::uint32 position_index;
    const glm::uvec3 dimensions;
    const glm::vec3 sdf_voxel_size;
    const Box volume_bounds;
    const bool calculate_color;

    // outputs
    bool is_valid;
    std::vector<Vertex> vertices;
};

void DistanceFieldDumpTask::doWork() noexcept {
    const glm::uint32 brick_index = indirection_table[position_index];
    is_valid = brick_index != DistanceField::INVALID_BRICK_INDEX;

    if (!is_valid && calculate_color) return;

    const glm::uint32 brick_size = DistanceField::BRICK_SIZE * DistanceField::BRICK_SIZE * DistanceField::BRICK_SIZE;
    const glm::uint32 brick_size_bytes = brick_size * sizeof(glm::uint8);

    const glm::vec3 indirection_voxel_size = sdf_voxel_size * (float) DistanceField::UNIQUE_DATA_BRICK_SIZE;

    vertices.resize(brick_size);

    const glm::uvec3 brick_coordinate{
        position_index % dimensions.x,
        position_index / dimensions.x % dimensions.y,
        position_index / dimensions.x / dimensions.y % dimensions.z,
    };

    const glm::vec3 brick_min_position = volume_bounds.min + glm::vec3(brick_coordinate) * indirection_voxel_size;

    if (!calculate_color) {
        const uchar4 display_color = is_valid ? uchar4{200, 200, 200, 255} : uchar4{0, 0, 0, 255};
        for (glm::uint32 i = 0; i < brick_size; ++i) {
            const glm::uvec3 voxel_coordinate = {
                i % DistanceField::BRICK_SIZE,
                i / DistanceField::BRICK_SIZE % DistanceField::BRICK_SIZE,
                i / DistanceField::BRICK_SIZE / DistanceField::BRICK_SIZE,
            };
            const glm::vec3 sample_position = glm::vec3(voxel_coordinate) * sdf_voxel_size + brick_min_position;
            vertices[i] = {sample_position, display_color};
        }
    } else {
        for (glm::uint32 i = 0; i < brick_size; ++i) {
            const glm::uvec3 voxel_coordinate = {
                i % DistanceField::BRICK_SIZE,
                i / DistanceField::BRICK_SIZE % DistanceField::BRICK_SIZE,
                i / DistanceField::BRICK_SIZE / DistanceField::BRICK_SIZE,
            };

            const glm::vec3 sample_position = glm::vec3(voxel_coordinate) * sdf_voxel_size + brick_min_position;
            const glm::uint8 distance = glm::uint8(255) - brick_data[std::size_t(brick_index) * brick_size_bytes + i];
            const auto gamma_color = glm::uint8(std::pow(float(distance) / 255.0f, 1.0f / 2.2f) * 255.0f);

            vertices[i] = {sample_position, uchar4{gamma_color, gamma_color, gamma_color, 255}};
        }
    }
}

ArgParser const &arg_parser = ArgParser::getInstance();

void dump_vertex(const char *filename, std::vector<Vertex> const &vertices) {
    assert(filename != nullptr);

    FILE *output_file = fopen(filename, "wb");
    if (output_file == nullptr) fmt::print(stderr, "Failed to open {}\n", filename);
    const std::size_t vertex_count = vertices.size();

    // write ply header
    if constexpr (std::endian::native == std::endian::little) {
        fmt::print(output_file, "ply\nformat binary_little_endian 1.0\n");
    } else {
        fmt::print(output_file, "ply\nformat binary_big_endian 1.0\n");
    }

    fmt::print(output_file, "element vertex {}\n", vertex_count);
    fmt::print(output_file, "property float x\nproperty float y\nproperty float z\n");
    fmt::print(output_file, "property uchar red\nproperty uchar green\nproperty uchar blue\nproperty uchar alpha\n");
    fmt::print(output_file, "end_header\n");
    // write vertices
    std::fwrite(vertices.data(), sizeof(Vertex), vertex_count, output_file);
    std::fclose(output_file);
}

} // namespace

bool dump_sdf_volume_for_visualization(DistanceFieldVolumeData const &volume_data) noexcept try {
    Box const &mesh_bounds = volume_data.local_space_mesh_bounds;

    for (glm::uint32 mip_index = 0; mip_index < DistanceField::NUM_MIPS; ++mip_index) {
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

        const glm::vec3 distance_field_voxel_size = mesh_bounds.getSize() / glm::vec3(dimensions * DistanceField::UNIQUE_DATA_BRICK_SIZE -
                                                                                      2 * DistanceField::MESH_DISTANCE_FIELD_OBJECT_BORDER);
        const Box distance_field_volume_bounds = mesh_bounds.expandBy(distance_field_voxel_size);
        const glm::vec3 indirection_voxel_size = distance_field_voxel_size * (float) DistanceField::UNIQUE_DATA_BRICK_SIZE;

        std::vector<DistanceFieldDumpTask> dump_tasks;

        dump_tasks.reserve(indirection_table_size / 8);

        for (glm::uint32 position_index = 0; position_index < indirection_table_size; ++position_index) {
            const glm::uint32 brick_offset = indirection_table[position_index];
            const bool is_valid_brick = brick_offset != DistanceField::INVALID_BRICK_INDEX;

            dump_tasks.emplace_back(indirection_table, brick_data, position_index, dimensions, distance_field_voxel_size,
                                    distance_field_volume_bounds, !arg_parser.debug_brick);
        }

        std::for_each(std::execution::par_unseq, dump_tasks.begin(), dump_tasks.end(),
                      [](DistanceFieldDumpTask &task) noexcept { task.doWork(); });

        if (arg_parser.debug_brick) {
            std::vector<Vertex> valid_vertices;
            std::vector<Vertex> invalid_vertices;

            valid_vertices.reserve(indirection_table_size * brick_size / 8);
            invalid_vertices.reserve(indirection_table_size * brick_size / 8);

            for (const auto &dump_task : dump_tasks) {
                auto const &vertices = dump_task.vertices;
                auto &target_buffer = dump_task.is_valid ? valid_vertices : invalid_vertices;
                std::ranges::copy(vertices, std::back_inserter(target_buffer));
            }

            dump_vertex(fmt::format("{}{}_valid_bricks.ply", arg_parser.output_filename, mip_index).c_str(), valid_vertices);
            dump_vertex(fmt::format("{}{}_invalid_bricks.ply", arg_parser.output_filename, mip_index).c_str(), invalid_vertices);

        } else {
            std::vector<Vertex> vertices;
            vertices.reserve(indirection_table_size * brick_size / 8);
            for (auto const &dump_task : dump_tasks) {
                std::ranges::copy(dump_task.vertices, std::back_inserter(vertices));
            }
            dump_vertex(fmt::format("{}{}_color.ply", arg_parser.output_filename, mip_index).c_str(), vertices);
        }
    }

    return true;
} catch (...) {
    return false;
}