#pragma once

#include <fmt/format.h>
#include <glm/vec3.hpp>

template <>
struct fmt::formatter<glm::vec3> : formatter<std::string> {
    template <typename CTX>
    auto format(const glm::vec3 &v, CTX &ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "{} {} {}", v.x, v.y, v.z);
    }
};