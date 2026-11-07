#include "runtime/TerraformMath.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

using Spore::Terrain::PlanetTypeFromTerraformValue;
using Spore::Terrain::TerraformBrushFalloff;

namespace {
void Expect(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

void TestPlanetType() {
  constexpr float lava = 0.75f;
  Expect(PlanetTypeFromTerraformValue(-10.0f, lava) == 2, "negative terraform value");
  Expect(PlanetTypeFromTerraformValue(0.0f, lava) == 2, "type 2");
  Expect(PlanetTypeFromTerraformValue(0.1f, lava) == 3, "0.1 belongs to type 3");
  Expect(PlanetTypeFromTerraformValue(0.39f, lava) == 3, "intermediate type 3");
  Expect(PlanetTypeFromTerraformValue(0.4f, lava) == 1, "0.4 belongs to type 1");
  Expect(PlanetTypeFromTerraformValue(0.59f, lava) == 1, "intermediate type 1");
  Expect(PlanetTypeFromTerraformValue(0.6f, lava) == 5, "between 0.6 and lava");
  Expect(PlanetTypeFromTerraformValue(0.74f, lava) == 5, "below lava threshold");
  Expect(PlanetTypeFromTerraformValue(0.75f, lava) == 4, "at lava threshold");
  Expect(PlanetTypeFromTerraformValue(1.0f, lava) == 4, "above lava threshold");
  Expect(PlanetTypeFromTerraformValue(0.6f, 0.6f) == 4, "lava threshold equal");
  Expect(PlanetTypeFromTerraformValue(0.6f, 0.5f) == 4, "lava threshold below");
  Expect(PlanetTypeFromTerraformValue(0.7f, 1.1f) == 5, "threshold above 1");
  Expect(PlanetTypeFromTerraformValue(
             std::numeric_limits<float>::infinity(), lava) == 4, "infinity type");
  Expect(PlanetTypeFromTerraformValue(
             -std::numeric_limits<float>::infinity(), lava) == 2,
         "negative infinity type");
  Expect(PlanetTypeFromTerraformValue(
             std::numeric_limits<float>::quiet_NaN(), lava) == 5,
         "NaN follows retail branch ordering");
  Expect(PlanetTypeFromTerraformValue(
             0.8f, std::numeric_limits<float>::quiet_NaN()) == 5,
         "NaN lava threshold leaves type 5");
  for (unsigned i = 0; i <= 1000; ++i) {
    const float f = float(i) / 1000;
    const auto type = PlanetTypeFromTerraformValue(f, lava);
    Expect(type == 1 || type == 2 || type == 3 || type == 4 || type == 5,
           "planet type is one of the recovered five values");
  }
}

void TestBrush() {
  Expect(TerraformBrushFalloff(0.5f, 0.5f) == 1.0f, "centre has full strength");
  Expect(TerraformBrushFalloff(0.0f, 0.0f) >= 0.0f, "corner never negative");
  Expect(TerraformBrushFalloff(0.0f, 0.0f) < 0.00001f, "corner is approximately zero");
  Expect(TerraformBrushFalloff(10.0f, 10.0f) == 0.0f, "outside circle clamps zero");
  Expect(TerraformBrushFalloff(0.5f,
             std::numeric_limits<float>::infinity()) == 0.0f,
         "infinite distance clamps zero");
  Expect(TerraformBrushFalloff(
             std::numeric_limits<float>::quiet_NaN(), 0.5f) == 0.0f,
         "NaN propagates to zero after ordered comparison");
  for (unsigned y = 0; y <= 100; ++y) {
    for (unsigned x = 0; x <= 100; ++x) {
      const float a = float(x) / 100, b = float(y) / 100;
      const float v = TerraformBrushFalloff(a, b);
      const float mirror = TerraformBrushFalloff(b, a);
      Expect(std::isfinite(v) && v >= 0.0f && v <= 1.0f,
             "falloff constrained to [0, 1]");
      Expect(v == mirror, "radial falloff symmetric in x/y");
    }
  }
  for (unsigned i = 1; i <= 500; ++i) {
    const float prev = TerraformBrushFalloff(0.5f - float(i - 1) / 1000, 0.5f);
    const float next = TerraformBrushFalloff(0.5f - float(i) / 1000, 0.5f);
    Expect(next <= prev, "falloff never rises when moving away from centre");
  }
}

} // namespace

int main() {
  try {
    TestPlanetType();
    TestBrush();
    std::cout << "Portable migrated Spore terraform math regression tests passed.\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Terraform test failed: " << e.what() << '\n';
    return 1;
  }
}
