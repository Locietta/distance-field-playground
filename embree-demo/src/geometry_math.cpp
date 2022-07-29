#include "geometry_math.h"
#include <fmt/core.h>
#include <glm/ext/scalar_constants.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <random>
#include <utility>

Plane::Plane(glm::dvec3 const &point, glm::dvec3 const &normal) : plane_point_(point), normal_{glm::normalize(normal)} {}

Plane::Plane(glm::dvec3 const &A, glm::dvec3 const &B, glm::dvec3 const &C)
    : plane_point_{A}, normal_{glm::normalize(glm::cross(B - A, C - B))} {}

double Plane::planeDot(const glm::dvec3 &P) {
    return glm::dot(P - plane_point_, normal_);
}

glm::dvec3 Plane::pointProjection(glm::dvec3 const &P) {
    return P - glm::dot(P - plane_point_, normal_) * normal_;
}

glm::dvec3 closestPointOnSegment(glm::dvec3 const &P, glm::dvec3 const &start, glm::dvec3 const &end) {
    const glm::dvec3 segment = end - start;
    const glm::dvec3 vecToPoint = P - start;

    const double dot1 = glm::dot(vecToPoint, segment);
    if (dot1 <= 0) {
        return start;
    }

    const double dot2 = glm::dot(segment, segment);
    if (dot2 <= dot1) {
        return end;
    }

    return start + segment * (dot1 / dot2);
}

glm::dvec3 closestPointOnTriangle(glm::dvec3 const &P, glm::dvec3 const &A, glm::dvec3 const &B, glm::dvec3 const &C) {
    const glm::dvec3 BA = A - B;
    const glm::dvec3 AC = C - A;
    const glm::dvec3 CB = B - C;
    const glm::dvec3 normal = glm::normalize(glm::cross(BA, CB));

    Plane planes[3] = {
        {B, glm::cross(normal, BA)},
        {A, glm::cross(normal, AC)},
        {C, glm::cross(normal, CB)},
    };

    int planeHalfSpaceBitMask = 0;
    for (int i = 0; i < 3; ++i) {
        if (planes[i].planeDot(P) > 0.0f) {
            planeHalfSpaceBitMask |= (1 << i);
        }
    }

    switch (planeHalfSpaceBitMask) {
    case 0: // 000 Inside
        return Plane(A, normal).pointProjection(P);
    case 1: // 001 Segment BA
        return closestPointOnSegment(P, B, A);
    case 2: // 010 Segment AC
        return closestPointOnSegment(P, A, C);
    case 3: // 011 Point A
        return A;
    case 4: // 100 Segment BC
        return closestPointOnSegment(P, B, C);
    case 5: // 101 Point B
        return B;
    case 6: // 110 Point C
        return C;
    default: fmt::print(stderr, "Impossible result in closestPointOnTriangle\n"); break;
    }

    // should be unreachable
    return P;
}

// ---------- random samples -----------

std::random_device rd;
std::mt19937 prng{rd()};
std::uniform_real_distribution<float> real_dist(0, 1);

glm::vec3 uniformHemisphereSamples(glm::vec2 uniforms) {
    uniforms = uniforms * 2.0f - 1.0f;
    if (uniforms == glm::vec2(0.0f)) {
        return {0, 0, 0};
    }

    float r, theta;

    if (std::fabs(uniforms.x) > std::fabs(uniforms.y)) {
        r = uniforms.x;
        theta = glm::pi<float>() / 4 * (uniforms.y / uniforms.x);
    } else {
        r = uniforms.y;
        theta = glm::pi<float>() / 2 - glm::pi<float>() / 4 * (uniforms.x / uniforms.y);
    }

    const float U = r * std::cos(theta);
    const float V = r * std::sin(theta);
    const float r2 = r * r;

    // map to hemisphere [P. Shirley, Kenneth Chiu; 1997; A Low Distortion Map Between Disk and Square]
    return {U * std::sqrt(2 - r2), V * std::sqrt(2 - r2), 1.0f - r2};
}

std::vector<glm::vec3> stratifiedUniformHemisphereSamples(int num_samples) {
    const auto num_samples_dim = (std::size_t) std::sqrt(num_samples);
    std::vector<glm::vec3> res(num_samples_dim * num_samples_dim);

    for (size_t x_index = 0; x_index < num_samples_dim; ++x_index) {
        for (size_t y_index = 0; y_index < num_samples_dim; ++y_index) {
            const float u1 = real_dist(prng);
            const float u2 = real_dist(prng);

            const float frac1 = ((float) x_index + u1) / (float) num_samples_dim;
            const float frac2 = ((float) x_index + u2) / (float) num_samples_dim;

            res[x_index * num_samples_dim + y_index] = uniformHemisphereSamples({frac1, frac2});
        }
    }

    return res;
}
