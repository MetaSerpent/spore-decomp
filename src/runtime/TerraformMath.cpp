#include "runtime/TerraformMath.h"

#include <cmath>

namespace Spore::Terrain {

uint32_t PlanetTypeFromTerraformValue(float f, float lavaThreshold) {
  // Branch conditions preserve the order of migrated function @ 0x00FC1FD0,
  // including its behaviour for NaNs and non-normalized values.
  if (f < 0.1f) return 2;
  if (f >= 0.1f && f < 0.4f) return 3;
  if (f >= 0.4f && f < 0.6f) return 1;
  if (f < 0.6f || lavaThreshold <= f) return 4;
  return 5;
}

float TerraformBrushFalloff(float x, float y) {
  // Original @ 0x00FC2040 computes this expression in x87, clamps negative
  // values to zero, and returns zero for NaN because 0 <= NaN is false.
  // Preserve float inputs, double intermediate squares, and FLOAT factor.
  const double dx = static_cast<double>(x) - 0.5;
  const double dy = static_cast<double>(y) - 0.5;
  const float f = static_cast<float>(
      1.0 - std::sqrt(dy * dy + dx * dx) *
                static_cast<double>(1.4142135f));
  return 0.0f <= f ? f : 0.0f;
}

} // namespace Spore::Terrain
