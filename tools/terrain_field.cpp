// Headless, asset-free terrain runtime built from recovered Spore brush math.
#include "runtime/TerraformField.h"

#include <charconv>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>

using Spore::Terrain::TerraformField;
namespace {
void Usage() {
  std::cerr << "Usage:\n"
               "  spore_terrain_field run <width> <height> <ticks> <seed> <radius> <delta> [snapshot.txt]\n"
               "  spore_terrain_field inspect <snapshot.txt>\n"
               "Coordinates are generated deterministically from the seed. No game assets required.\n";
}
template<typename T> T Unsigned(const char* input) {
  const std::string str(input);
  if (str.empty() || str.front() == '-') throw std::invalid_argument("unsigned number expected");
  T value = 0;
  const auto r = std::from_chars(str.data(), str.data() + str.size(), value);
  if (r.ec != std::errc{} || r.ptr != str.data() + str.size())
    throw std::invalid_argument("invalid or overflowing unsigned number");
  return value;
}
float Real(const char* input) {
  errno = 0;
  char* end = nullptr;
  const float value = std::strtof(input, &end);
  if (end == input || *end != '\0' || errno == ERANGE)
    throw std::invalid_argument("invalid float argument");
  return value;
}
struct Random {
  std::uint64_t state;
  explicit Random(std::uint64_t seed)
      : state(seed == 0 ? UINT64_C(0x9E3779B97F4A7C15) : seed) {}
  std::uint64_t Next() {
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    return state * UINT64_C(2685821657736338717);
  }
  float Unit() {
    return static_cast<float>(static_cast<double>(Next() >> 11) *
                              (1.0 / 9007199254740992.0));
  }
};
std::string ReadFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot open terrain snapshot: " + path);
  const std::string data{std::istreambuf_iterator<char>(in),
                         std::istreambuf_iterator<char>()};
  if (!in.eof() && in.fail()) throw std::runtime_error("cannot read snapshot");
  if (data.size() > 20000000)
    throw std::invalid_argument("terrain snapshot exceeds 20MB limit");
  return data;
}
void SaveFile(const std::string& path, const std::string& value) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out || !out.write(value.data(), static_cast<std::streamsize>(value.size())))
    throw std::runtime_error("cannot write terrain snapshot: " + path);
}
void PrintStats(const TerraformField& field) {
  std::cout << "width,height,mean,planet_type,checksum\n"
            << field.Width() << ',' << field.Height() << ','
            << std::setprecision(std::numeric_limits<float>::max_digits10)
            << field.Mean() << ',' << field.PlanetType(0.75f) << ','
            << std::hex << field.Checksum() << std::dec << '\n';
}
}
int main(int argc, char** argv) {
  try {
    if (argc == 3 && std::string(argv[1]) == "inspect") {
      const auto field = TerraformField::Restore(ReadFile(argv[2]));
      PrintStats(field);
      return 0;
    }
    if ((argc == 8 || argc == 9) && std::string(argv[1]) == "run") {
      const auto width = Unsigned<std::uint32_t>(argv[2]);
      const auto height = Unsigned<std::uint32_t>(argv[3]);
      const auto steps = Unsigned<std::uint32_t>(argv[4]);
      const auto seed = Unsigned<std::uint64_t>(argv[5]);
      const auto radius = Real(argv[6]);
      const auto delta = Real(argv[7]);
      if (steps > 100000 ||
          std::uint64_t(width) * height * steps > UINT64_C(25000000))
        throw std::invalid_argument("run exceeds 25M cell-tick safety budget");
      TerraformField field(width, height);
      Random rng(seed);
      // Fail parameter validation before printing a single CSV row.
      field.Apply(0.5f, 0.5f, radius, 0.0f);
      if (delta < -1.0f || delta > 1.0f || !std::isfinite(delta))
        throw std::invalid_argument("invalid brush delta");
      std::cout << "tick,centre_x,centre_y,affected,applied_delta,mean,planet_type,checksum\n";
      std::cout << std::setprecision(std::numeric_limits<float>::max_digits10);
      for (std::uint32_t tick = 1; tick <= steps; ++tick) {
        const float x = rng.Unit(), y = rng.Unit();
        const auto change = field.Apply(x, y, radius, delta);
        std::cout << tick << ',' << x << ',' << y << ',' << change.affected
                  << ',' << change.appliedDelta << ',' << field.Mean()
                  << ',' << field.PlanetType(0.75f) << ','
                  << std::hex << field.Checksum() << std::dec << '\n';
      }
      if (argc == 9) SaveFile(argv[8], field.Snapshot());
      return 0;
    }
    Usage();
    return 2;
  } catch (const std::exception& e) {
    std::cerr << "spore_terrain_field: " << e.what() << '\n';
    return 1;
  }
}
