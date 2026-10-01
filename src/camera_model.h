#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

// The pinhole camera the guidelines are projected through. For the overlay to land on the real
// ground these must match the physical camera: mounting height, downward pitch, and lens FOV.
//
// World frame (shared with guidelines.h), origin on the ground directly below the camera:
//   +x to the side, +y up, +z straight back from the bumper.
struct CameraModel {
  float heightM = 1.0f;
  float pitchDeg = 30.0f;
  // Backup cameras use very wide lenses (often 130 deg+ fisheye). A pinhole model starts to
  // stretch badly past ~110 deg, so 100 is the default until fisheye correction lands.
  float horizontalFovDeg = 100.0f;

  // World -> clip space for an image of the given width/height aspect.
  glm::mat4 viewProjection(float imageAspect) const {
    const float pitch = glm::radians(pitchDeg);
    const glm::vec3 eye{0.0f, heightM, 0.0f};
    const glm::vec3 forward{0.0f, -std::sin(pitch), std::cos(pitch)};
    const glm::mat4 view = glm::lookAt(eye, eye + forward, glm::vec3{0.0f, 1.0f, 0.0f});
    const float hfov = glm::radians(horizontalFovDeg);
    const float vfov = 2.0f * std::atan(std::tan(hfov * 0.5f) / imageAspect);
    return glm::perspective(vfov, imageAspect, 0.05f, 100.0f) * view;
  }
};
