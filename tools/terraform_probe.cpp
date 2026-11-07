// Offline demonstration of a portable gameplay function reconstructed from Spore.
#include "runtime/TerraformMath.h"

#include <charconv>
#include <cerrno>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <system_error>

namespace {
void Usage() {
  std::cerr << "Usage:\n"
               "  spore_terraform_probe grid <side: 2..256>\n"
               "  spore_terraform_probe type <value> <lavaThreshold>\n";
}
bool ParseSize(const char* text, unsigned& value) {
  const std::string s(text);
  auto parsed = std::from_chars(s.data(), s.data() + s.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == s.data() + s.size();
}
bool ParseFloat(const char* text, float& value) {
  // std::strtof supports the same syntax on common hosted C runtimes.
  char* end = nullptr;
  errno = 0;
  value = std::strtof(text, &end);
  return end != text && *end == '\0' && errno == 0;
}
} // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "grid") {
    unsigned side = 0;
    if (!ParseSize(argv[2], side) || side < 2 || side > 256) {
      Usage();
      return 2;
    }
    std::cout << "x,y,falloff\n";
    std::cout << std::setprecision(std::numeric_limits<float>::max_digits10);
    for (unsigned y = 0; y < side; ++y) {
      for (unsigned x = 0; x < side; ++x) {
        const float fx = float(x) / float(side - 1);
        const float fy = float(y) / float(side - 1);
        std::cout << fx << ',' << fy << ','
                  << Spore::Terrain::TerraformBrushFalloff(fx, fy) << '\n';
      }
    }
    return 0;
  }
  if (argc == 4 && std::string(argv[1]) == "type") {
    float f = 0, lava = 0;
    if (!ParseFloat(argv[2], f) || !ParseFloat(argv[3], lava)) {
      Usage();
      return 2;
    }
    std::cout << Spore::Terrain::PlanetTypeFromTerraformValue(f, lava) << '\n';
    return 0;
  }
  Usage();
  return 2;
}
