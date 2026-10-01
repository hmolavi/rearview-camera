#pragma once

#include <glm/glm.hpp>

#include <vector>

// Dynamic parking guidelines, built as triangles on the ground plane (y = 0) in metres.
//
// World frame, origin on the ground directly below the rear camera:
//   +x  to the side,  +y  up,  +z  straight back from the bumper (the direction the car reverses).
//
// Lines are real ribbons of width `lineWidth` lying on the ground. Projecting them through the
// camera then gives correct perspective for free: far segments get thinner and converge, exactly
// like paint on asphalt. (It also sidesteps glLineWidth, which core-profile GL caps at 1 px.)

struct Vehicle {
  float wheelbase = 2.9f;     // front axle to rear axle (m)
  float rearOverhang = 1.0f;  // rear axle to bumper / camera (m)
  float trackWidth = 1.9f;    // distance between the two guideline paths (m)
  float maxSteerDeg = 35.0f;  // front-wheel angle limit
};

struct GuidelineStyle {
  float lineWidth = 0.08f;                  // m
  float length = 4.0f;                      // how far back the path is drawn (m)
  float bandEdges[3] = {1.0f, 2.0f, 4.0f};  // red until 1 m, yellow until 2 m, green after
  float tickLength = 0.35f;                 // inward ticks at each band edge (m)
  int samplesPerMetre = 12;
};

struct GuideVertex {
  glm::vec3 position;
  glm::vec4 color;
};

// Rebuilds the triangle list for a given front-wheel steering angle (degrees, +ve curves toward +x).
std::vector<GuideVertex> buildGuidelines(float steerDeg, const Vehicle& vehicle,
                                         const GuidelineStyle& style);
