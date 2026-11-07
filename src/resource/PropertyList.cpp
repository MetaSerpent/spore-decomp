#include "resource/PropertyList.h"

#include <cstring>
#include <limits>
#include <string>
#include <utility>

namespace Spore::Resource {
namespace {

constexpr uint32_t kMaxProperties = 1U << 16;
constexpr uint32_t kMaxElements = 1U << 20;
constexpr uint32_t kMaxStringUnits = 1U << 20;
constexpr uint32_t kMaxItemSize = 1U << 20;

struct Reader {
  const uint8_t* data;
  size_t size;
  size_t at = 0;

  bool Has(size_t n, size_t limit) const {
    return at <= size && limit <= size && at <= limit && n <= limit - at;
  }
  bool BE16(uint16_t& v, size_t limit) {
    if (!Has(2, limit)) return false;
    v = uint16_t(uint16_t(data[at]) << 8 | data[at + 1]);
    at += 2;
    return true;
  }
  bool BE32(uint32_t& v, size_t limit) {
    if (!Has(4, limit)) return false;
    v = (uint32_t(data[at]) << 24) | (uint32_t(data[at + 1]) << 16) |
        (uint32_t(data[at + 2]) << 8) | uint32_t(data[at + 3]);
    at += 4;
    return true;
  }
  bool LE32(uint32_t& v, size_t limit) {
    if (!Has(4, limit)) return false;
    v = uint32_t(data[at]) | (uint32_t(data[at + 1]) << 8) |
        (uint32_t(data[at + 2]) << 16) | (uint32_t(data[at + 3]) << 24);
    at += 4;
    return true;
  }
  bool Skip(size_t n, size_t limit) {
    if (!Has(n, limit)) return false;
    at += n;
    return true;
  }
};

bool ReadString16(Reader& r, size_t limit, std::u16string& value) {
  uint32_t chars = 0;
  if (!r.LE32(chars, limit) || chars > kMaxStringUnits ||
      size_t(chars) > (limit - r.at) / 2) return false;
  value.clear();
  value.reserve(chars);
  for (uint32_t i = 0; i < chars; ++i) {
    const auto lo = uint16_t(r.data[r.at]);
    const auto hi = uint16_t(r.data[r.at + 1]);
    value.push_back(static_cast<char16_t>(lo | uint16_t(hi << 8)));
    r.at += 2;
  }
  return true;
}

bool ReadKey(Reader& r, size_t limit, ResourceKey& key) {
  // The on-disk order is instance, type, group, independent of ResourceKey layout.
  return r.LE32(key.instance, limit) && r.LE32(key.type, limit) &&
         r.LE32(key.group, limit);
}

bool ReadVector3(Reader& r, size_t limit, Vector3& v) {
  uint32_t bits[3] = {};
  if (!r.LE32(bits[0], limit) || !r.LE32(bits[1], limit) ||
      !r.LE32(bits[2], limit)) return false;
  static_assert(sizeof(float) == sizeof(uint32_t), "requires IEEE 754 binary32 storage");
  std::memcpy(&v.x, &bits[0], 4);
  std::memcpy(&v.y, &bits[1], 4);
  std::memcpy(&v.z, &bits[2], 4);
  return true;
}

bool ReadValue(Reader& r, Property& property) {
  if (!property.IsArray()) {
    if (property.type == PropertyList::kString16) {
      std::u16string value;
      if (!ReadString16(r, r.size, value)) return false;
      property.value = std::move(value);
      return true;
    }
    if (property.type == PropertyList::kKey) {
      ResourceKey key;
      // The original scalar representation uses 16 bytes, including padding.
      if (!ReadKey(r, r.size, key) || !r.Skip(4, r.size)) return false;
      property.value = key;
      return true;
    }
    if (property.type == PropertyList::kVector3) {
      Vector3 value;
      if (!ReadVector3(r, r.size, value) || !r.Skip(4, r.size)) return false;
      property.value = value;
      return true;
    }
    return false;
  }

  uint32_t count = 0;
  uint32_t itemSize = 0;
  if (!r.BE32(count, r.size) || !r.BE32(itemSize, r.size) ||
      count > kMaxElements || itemSize > kMaxItemSize) return false;

  // Fixed-stride arrays must fit *before* allocating or reading any values.
  // String16's zero-stride form is permitted for sequential variable-size strings.
  if (itemSize == 0) {
    if (property.type != PropertyList::kString16 && count != 0) return false;
  } else if (size_t(count) > (r.size - r.at) / itemSize) {
    return false;
  }

  if (property.type == PropertyList::kString16) {
    std::vector<std::u16string> values;
    values.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
      const size_t end = itemSize ? r.at + itemSize : r.size;
      std::u16string value;
      if (!ReadString16(r, end, value) || !r.Skip(end - r.at, end))
        return false;
      values.push_back(std::move(value));
    }
    property.value = std::move(values);
    return true;
  }

  if ((property.type != PropertyList::kKey && property.type != PropertyList::kVector3) ||
      (count != 0 && itemSize < 12)) return false;

  if (property.type == PropertyList::kKey) {
    std::vector<ResourceKey> values;
    values.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
      const size_t end = r.at + itemSize;
      ResourceKey key;
      if (!ReadKey(r, end, key) || !r.Skip(end - r.at, end)) return false;
      values.push_back(key);
    }
    property.value = std::move(values);
  } else {
    std::vector<Vector3> values;
    values.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
      const size_t end = r.at + itemSize;
      Vector3 vec;
      if (!ReadVector3(r, end, vec) || !r.Skip(end - r.at, end)) return false;
      values.push_back(vec);
    }
    property.value = std::move(values);
  }
  return true;
}
} // namespace

bool PropertyList::Parse(const uint8_t* data, size_t size) {
  Clear();
  if (!data || size < 4) {
    error_ = "truncated property-list header";
    return false;
  }
  Reader r{data, size};
  uint32_t count = 0;
  if (!r.BE32(count, size) || count > kMaxProperties ||
      size_t(count) > (size - r.at) / 8) {
    error_ = "invalid property count";
    return false;
  }
  std::vector<Property> parsed;
  parsed.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    Property p;
    if (!r.BE32(p.id, size) || !r.BE16(p.type, size) ||
        !r.BE16(p.flags, size) || !ReadValue(r, p)) {
      error_ = "unsupported or truncated property #" + std::to_string(i) +
               " at byte offset " + std::to_string(r.at);
      return false;
    }
    parsed.push_back(std::move(p));
  }
  if (r.at != size) {
    error_ = "trailing bytes after property list: " + std::to_string(size - r.at);
    return false;
  }
  properties_ = std::move(parsed);
  return true;
}

void PropertyList::Clear() {
  properties_.clear();
  error_.clear();
}

const Property* PropertyList::Find(uint32_t id) const {
  for (const auto& p : properties_)
    if (p.id == id) return &p;
  return nullptr;
}

} // namespace Spore::Resource
