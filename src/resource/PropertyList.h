#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "resource/DBPF.h"

namespace Spore::Resource {

// A deliberately conservative, bounds-checked subset of Spore .prop files.
// Format evidence: docs/formats.md. Types not yet established are rejected
// explicitly rather than guessing their sizes and desynchronizing the stream.
struct Vector3 { float x = 0, y = 0, z = 0; };
using PropertyValue = std::variant<
    std::u16string, ResourceKey, Vector3,
    std::vector<std::u16string>, std::vector<ResourceKey>, std::vector<Vector3>>;

struct Property {
  uint32_t id = 0;
  uint16_t type = 0;
  uint16_t flags = 0;
  PropertyValue value;
  bool IsArray() const { return (flags & 0x30U) != 0; }
};

class PropertyList {
 public:
  static constexpr uint16_t kString16 = 0x13;
  static constexpr uint16_t kKey = 0x20;
  static constexpr uint16_t kVector3 = 0x31;

  // Replaces old contents only on success. On error contents are cleared.
  // Input can be returned by PackageStack::Read for type ID 0x00B1B104.
  bool Parse(const uint8_t* data, size_t size);
  bool Parse(const std::vector<uint8_t>& bytes) {
    return Parse(bytes.data(), bytes.size());
  }
  void Clear();
  const std::vector<Property>& Properties() const { return properties_; }
  const Property* Find(uint32_t id) const;
  const std::string& Error() const { return error_; }

 private:
  std::vector<Property> properties_;
  std::string error_;
};

} // namespace Spore::Resource
