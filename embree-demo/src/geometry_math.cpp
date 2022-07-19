#include "geometry_math.h"
#include <fmt/core.h>
#include <glm/geometric.hpp>
#include <utility>

Plane::Plane(glm::dvec3 const &point, glm::dvec3 const &normal)
    : plane_point_(point), normal_{glm::normalize(normal)} {}

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
