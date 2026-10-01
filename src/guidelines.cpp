#include "guidelines.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace {

const glm::vec4 kRed{0.95f, 0.15f, 0.10f, 0.95f};
const glm::vec4 kYellow{1.00f, 0.85f, 0.10f, 0.95f};
const glm::vec4 kGreen{0.20f, 0.90f, 0.30f, 0.95f};

// Where a bumper point ends up after the rear axle has reversed `s` metres.
//
// Kinematic bicycle model: at front-wheel angle d the car rotates about an instantaneous centre
// of rotation (ICR) on the rear-axle line, offset sideways by R = wheelbase / tan(d). Every point
// on the body sweeps a circle around that same ICR, so the left and right guidelines are two
// concentric arcs. The rear axle sits `rearOverhang` in front of the camera, i.e. at z = -overhang.
glm::vec2 pathPoint(glm::vec2 start, float s, float radius, float overhang) {
  if (std::isinf(radius)) return {start.x, start.y + s};  // wheels straight: straight lines

  const glm::vec2 icr{radius, -overhang};
  // The rear axle moves at |R| * angular rate, so it has turned s / R radians. The sign makes the
  // car move toward +z (backwards) for either steering direction.
  const float phi = -s / radius;
  const float c = std::cos(phi), sn = std::sin(phi);
  const glm::vec2 r = start - icr;
  return icr + glm::vec2{r.x * c - r.y * sn, r.x * sn + r.y * c};
}

void pushQuad(std::vector<GuideVertex>& out, glm::vec2 a0, glm::vec2 a1, glm::vec2 b0, glm::vec2 b1,
              const glm::vec4& color) {
  // a0/a1 are the two edges at one end of the ribbon, b0/b1 at the other. Two triangles.
  auto v = [&](glm::vec2 p) { out.push_back({{p.x, 0.0f, p.y}, color}); };
  v(a0), v(a1), v(b1);
  v(a0), v(b1), v(b0);
}

glm::vec4 bandColor(int band) { return band == 0 ? kRed : band == 1 ? kYellow : kGreen; }

}  // namespace

std::vector<GuideVertex> buildGuidelines(float steerDeg, const Vehicle& vehicle,
                                         const GuidelineStyle& style) {
  steerDeg = std::clamp(steerDeg, -vehicle.maxSteerDeg, vehicle.maxSteerDeg);
  const float steer = glm::radians(steerDeg);
  const float radius = std::abs(steer) < 1e-4f ? INFINITY : vehicle.wheelbase / std::tan(steer);

  const float half = vehicle.trackWidth * 0.5f;
  const float hw = style.lineWidth * 0.5f;
  const float edges[4] = {0.0f, style.bandEdges[0], style.bandEdges[1],
                          std::min(style.bandEdges[2], style.length)};

  std::vector<GuideVertex> out;
  out.reserve(static_cast<size_t>(style.length * style.samplesPerMetre) * 12 + 64);

  for (float side : {-half, +half}) {
    const glm::vec2 start{side, 0.0f};

    // Each band is generated separately so the colour change lands exactly on the band edge.
    for (int band = 0; band < 3; ++band) {
      const float s0 = edges[band], s1 = edges[band + 1];
      if (s1 <= s0) continue;
      const int steps = std::max(1, static_cast<int>((s1 - s0) * style.samplesPerMetre));
      const glm::vec4 color = bandColor(band);

      glm::vec2 prevL{}, prevR{};
      for (int i = 0; i <= steps; ++i) {
        const float s = s0 + (s1 - s0) * static_cast<float>(i) / steps;
        const glm::vec2 p = pathPoint(start, s, radius, vehicle.rearOverhang);
        // Tangent by a small forward difference; the ribbon edges sit +-hw along its normal.
        const glm::vec2 t = glm::normalize(pathPoint(start, s + 0.01f, radius, vehicle.rearOverhang) - p);
        const glm::vec2 n{-t.y, t.x};
        const glm::vec2 l = p + n * hw, r = p - n * hw;
        if (i > 0) pushQuad(out, prevL, prevR, l, r, color);
        prevL = l;
        prevR = r;
      }

      // An inward tick where this band ends, like the distance markers on a real backup camera.
      const glm::vec2 p = pathPoint(start, s1, radius, vehicle.rearOverhang);
      const glm::vec2 other = pathPoint({-side, 0.0f}, s1, radius, vehicle.rearOverhang);
      const glm::vec2 inward = glm::normalize(other - p);
      const glm::vec2 along{inward.y, -inward.x};
      const glm::vec2 a = p, b = p + inward * style.tickLength;
      pushQuad(out, a - along * hw, a + along * hw, b - along * hw, b + along * hw, color);
    }
  }
  return out;
}
