#pragma once

#include "runtime/TerraformField.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Spore::Creature {

// New deterministic grid navigation for the BlackSpore runtime. NOT a recovered
// retail Spore navigation function. Elevation is the normalized terrain field.
struct PathOptions {
  float minimumHeight = 0.12f; // tiles below this are impassable water
  float maximumStep = 0.20f;   // maximum rise/drop between adjacent tiles
  float slopePenalty = 4.0f;   // extra movement cost for elevation change
};
struct Waypoint { uint32_t x=0, z=0; };
struct PathResult {
  std::vector<Waypoint> nodes; // inclusive start -> goal, or empty if unreachable
  uint64_t costMilli = 0;      // integer units, 1000 per flat cardinal step
  size_t expanded = 0;        // nodes expanded by A*
};
class NavigationGrid {
 public:
  // Strict limits: at least 2x2 and no more than 262144 samples, finite
  // heights in [0,1], finite constrained navigation parameters.
  NavigationGrid(uint32_t width,uint32_t height,
                 std::vector<float> heights,PathOptions options = {});
  static NavigationGrid FromTerrain(const Terrain::TerraformField& field,
                                    PathOptions options = {});

  uint32_t Width() const noexcept { return width_; }
  uint32_t Height() const noexcept { return height_; }
  bool Walkable(uint32_t x,uint32_t z) const noexcept;
  bool CanTraverse(Waypoint a,Waypoint b) const noexcept;
  PathResult Route(Waypoint start,Waypoint goal) const;
 private:
  uint32_t width_,height_;
  std::vector<float> heights_;
  PathOptions options_;
  uint32_t Index(uint32_t x,uint32_t z) const noexcept { return z*width_+x; }
};

} // namespace Spore::Creature
