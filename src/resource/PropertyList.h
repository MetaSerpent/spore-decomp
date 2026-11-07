#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "resource/DBPF.h"

namespace Spore::Resource {

// Observed Spore .prop types, with conservative bounds checking and no file writes.
// See docs/formats.md and the cited Spore property-format research.
struct Vector2 { float x = 0, y = 0; };
struct Vector3 { float x = 0, y = 0, z = 0; };
struct Vector4 { float x = 0, y = 0, z = 0, w = 0; };
struct BoundingBox { Vector3 minimum, maximum; };
struct LocalizedText { uint32_t table = 0, id = 0; std::u16string placeholder; };

using PropertyElement = std::variant<
    bool, int32_t, uint32_t, float,
    std::string, std::u16string, ResourceKey,
    Vector2, Vector3, Vector4, BoundingBox, LocalizedText>;

struct Property {
  uint32_t id = 0;
  uint16_t type = 0;
  uint16_t flags = 0;
  // Scalar properties have one value; array properties have zero or more.
  // Keeping values homogeneous without a union permits safe validation.
  std::vector<PropertyElement> values;
  bool IsArray() const { return (flags & 0x30U) != 0 || type == 0x22 || type == 0x39; }
};

class PropertyList {
 public:
  static constexpr uint16_t kBool = 0x01;
  static constexpr uint16_t kInt32 = 0x09;
  static constexpr uint16_t kUInt32 = 0x0A;
  static constexpr uint16_t kFloat = 0x0D;
  static constexpr uint16_t kString8 = 0x12;
  static constexpr uint16_t kString16 = 0x13;
  static constexpr uint16_t kKey = 0x20;
  static constexpr uint16_t kText = 0x22;
  static constexpr uint16_t kVector2 = 0x30;
  static constexpr uint16_t kVector3 = 0x31;
  static constexpr uint16_t kColorRGB = 0x32;
  static constexpr uint16_t kVector4 = 0x33;
  static constexpr uint16_t kColorRGBA = 0x34;
  static constexpr uint16_t kBoundingBox = 0x39;

  // On error existing properties are cleared and Error() explains the failure.
  // Use with PackageStack::Read on resource type 0x00B1B104.
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
