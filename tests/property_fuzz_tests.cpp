// Deterministic mutation tests for the asset-free PropertyList parser.
// This is crash-resilience testing, not proof of parity with retail Spore.
#include "resource/PropertyList.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using Spore::Resource::PropertyList;

namespace {
void Expect(bool truth, const char* message) {
  if (!truth) throw std::runtime_error(message);
}
using Bytes = std::vector<uint8_t>;
void BE16(Bytes& dst, uint16_t value) {
  dst.push_back(uint8_t(value >> 8));
  dst.push_back(uint8_t(value));
}
void BE32(Bytes& dst, uint32_t value) {
  dst.push_back(uint8_t(value >> 24));
  dst.push_back(uint8_t(value >> 16));
  dst.push_back(uint8_t(value >> 8));
  dst.push_back(uint8_t(value));
}
Bytes ValidProperty() {
  Bytes bytes;
  BE32(bytes, 3);
  BE32(bytes, 1); BE16(bytes, PropertyList::kInt32); BE16(bytes, 0);
  BE32(bytes, 123);
  BE32(bytes, 2); BE16(bytes, PropertyList::kString16); BE16(bytes, 0);
  BE32(bytes, 3);
  bytes.insert(bytes.end(), {'a', 0, 'b', 0, 'c', 0});
  BE32(bytes, 3); BE16(bytes, PropertyList::kFloat); BE16(bytes, 0x30);
  BE32(bytes, 3); BE32(bytes, 4);
  BE32(bytes, 0x3F800000); BE32(bytes, 0x40000000);
  BE32(bytes, 0x40400000);
  return bytes;
}
void TestParserMutations() {
  PropertyList parser;
  const Bytes original = ValidProperty();
  Expect(parser.Parse(original), "valid baseline .prop fixture");
  Expect(parser.Properties().size() == 3, "baseline field count");

  std::mt19937_64 rng(0xC3115A1E55EEDBEEULL);
  constexpr size_t kTrials = 12000;
  size_t accepted = 0;
  size_t rejected = 0;
  for (size_t iteration = 0; iteration < kTrials; ++iteration) {
    Bytes input;
    switch (iteration % 6) {
      case 0: // Single-bit flips, including property header and array counts
        input = original;
        input[rng() % input.size()] ^= uint8_t(1U << (rng() % 8));
        break;
      case 1: // Truncations, including empty input
        input.assign(original.begin(), original.begin() + rng() % original.size());
        break;
      case 2: // Overlong appended trailing data
        input = original;
        for (size_t n = rng() % 32; n > 0; --n) input.push_back(uint8_t(rng()));
        break;
      case 3: // Random bytes at random lengths
        input.resize(rng() % 300);
        for (auto& b : input) b = uint8_t(rng());
        break;
      case 4: // Splicing arbitrary pieces from a valid sample
        input = original;
        if (input.size() > 1) {
          const auto from = size_t(rng() % input.size());
          const auto count = size_t(rng() % (input.size() - from + 1));
          input.erase(input.begin() + from, input.begin() + from + count);
        }
        break;
      default: // Gross count and type/flag corruption
        input = original;
        if (rng() & 1) {
          for (unsigned i = 0; i < 4; ++i) input[i] = 0xFF;
        } else {
          const size_t pos = size_t(rng() % input.size());
          input[pos] = uint8_t(rng());
        }
        break;
    }

    const bool ok = parser.Parse(input);
    if (ok) {
      ++accepted;
      Expect(parser.Error().empty(), "successful parse clears errors");
      Expect(parser.Properties().size() <= 65536, "bounded property count");
      for (const auto& property : parser.Properties()) {
        Expect(property.values.size() <= (1U << 20), "bounded property array");
        if (!property.IsArray())
          Expect(property.values.size() == 1, "scalars have one value");
      }
    } else {
      ++rejected;
      Expect(parser.Properties().empty(), "failed parse clears stale output");
      Expect(!parser.Error().empty(), "failed parse supplies diagnostic");
    }
  }
  Expect(accepted + rejected == kTrials && rejected > 0,
         "all deterministic inputs exercised");
  Expect(parser.Parse(original) && parser.Properties().size() == 3,
         "fuzzed parser can recover cleanly");
  std::cout << "PropertyList mutation tests: "
            << accepted << " accepted, " << rejected << " rejected.\n";
}
} // namespace

int main() {
  try {
    TestParserMutations();
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Property parser mutation failure: " << e.what() << '\n';
    return 1;
  }
}
