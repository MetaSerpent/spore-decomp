#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Spore::Terrain {

// A portable, deterministic *new* terrain-state layer that composes recovered
// retail terraforming functions. It is not yet the original planet simulation.
// Positions and brush radii use normalized [0,1] terrain coordinates.
struct BrushResult {
  std::size_t affected = 0;  // grid samples changed (not just visited)
  double appliedDelta = 0;  // sum of all actual, clamped sample changes
};

class TerraformField {
 public:
  // Dimensions at least 2x2; at most one million total samples.
  explicit TerraformField(std::uint32_t width, std::uint32_t height,
                          float initialValue = 0.5f);

  std::uint32_t Width() const noexcept { return width_; }
  std::uint32_t Height() const noexcept { return height_; }
  const std::vector<float>& Samples() const noexcept { return samples_; }
  float At(std::uint32_t x, std::uint32_t y) const;
  double Mean() const noexcept;
  std::uint32_t PlanetType(float lavaThreshold) const;

  // Full strength at centre, zero at radius, derived from original
  // TerraformBrushFalloff. Signed delta is clamped to [-1, 1].
  // Throws on invalid coordinates/radius/delta *before* modifying any sample.
  BrushResult Apply(float centreX, float centreY, float radius, float delta);

  // Stable textual snapshots and little-endian FNV-1a state checksum.
  // Snapshots contain no game assets and can be regression tested offline.
  std::string Snapshot() const;
  static TerraformField Restore(std::string_view snapshot);
  std::uint64_t Checksum() const noexcept;

 private:
  std::uint32_t width_;
  std::uint32_t height_;
  std::vector<float> samples_;
};

} // namespace Spore::Terrain
