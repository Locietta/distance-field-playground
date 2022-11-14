#pragma once

#include <glm/vec3.hpp>
#include <vector>

glm::dvec3 closest_point_on_segment(glm::dvec3 const &P, glm::dvec3 const &start, glm::dvec3 const &end);
glm::dvec3 closest_point_on_triangle(glm::dvec3 const &P, glm::dvec3 const &A, glm::dvec3 const &B, glm::dvec3 const &C);

class Plane {
public:
    Plane(glm::dvec3 const &point, glm::dvec3 const &normal);
    Plane(glm::dvec3 const &A, glm::dvec3 const &B, glm::dvec3 const &C);

    double planeDot(glm::dvec3 const &point);
    glm::dvec3 pointProjection(glm::dvec3 const &point);

private:
    glm::dvec3 plane_point_;
    glm::dvec3 normal_;
};

std::vector<glm::vec3> stratified_uniform_hemisphere_samples(int num_samples);