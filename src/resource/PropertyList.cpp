#include "resource/PropertyList.h"

#include <cstring>
#include <string>
#include <utility>

namespace Spore::Resource {
namespace {

constexpr uint32_t kMaxProperties = 65536;
constexpr uint32_t kMaxElements = 1U << 20;
constexpr uint32_t kMaxStringUnits = 1U << 20;
constexpr uint32_t kMaxStride = 1U << 20;

struct Reader {
  const uint8_t* data;
  size_t size;
  size_t at = 0;
  size_t Remaining(size_t end) const {
    return at <= end && end <= size ? end - at : 0;
  }
  bool Has(size_t n, size_t end) const {
    return at <= end && end <= size && n <= end - at;
  }
  bool Byte(uint8_t& value, size_t end) {
    if (!Has(1, end)) return false;
    value = data[at++];
    return true;
  }
  bool BE16(uint16_t& value, size_t end) {
    if (!Has(2, end)) return false;
    value = (uint16_t(data[at]) << 8) | data[at + 1];
    at += 2;
    return true;
  }
  bool LE16(uint16_t& value, size_t end) {
    if (!Has(2, end)) return false;
    value = uint16_t(data[at]) | (uint16_t(data[at + 1]) << 8);
    at += 2;
    return true;
  }
  bool BE32(uint32_t& value, size_t end) {
    if (!Has(4, end)) return false;
    value = (uint32_t(data[at]) << 24) | (uint32_t(data[at + 1]) << 16) |
            (uint32_t(data[at + 2]) << 8) | uint32_t(data[at + 3]);
    at += 4;
    return true;
  }
  bool LE32(uint32_t& value, size_t end) {
    if (!Has(4, end)) return false;
    value = uint32_t(data[at]) | (uint32_t(data[at + 1]) << 8) |
            (uint32_t(data[at + 2]) << 16) | (uint32_t(data[at + 3]) << 24);
    at += 4;
    return true;
  }
  bool Skip(size_t n, size_t end) {
    if (!Has(n, end)) return false;
    at += n;
    return true;
  }
};

static_assert(sizeof(float) == sizeof(uint32_t), "PropertyList requires 32-bit float");
float AsFloat(uint32_t bits) {
  float result;
  std::memcpy(&result, &bits, 4);
  return result;
}
int32_t AsInt32(uint32_t bits) {
  int32_t result;
  std::memcpy(&result, &bits, 4);
  return result;
}
bool ReadString8(Reader& r, size_t end, std::string& result) {
  uint32_t length = 0;
  if (!r.BE32(length, end) || length > kMaxStringUnits || !r.Has(length, end))
    return false;
  result.assign(reinterpret_cast<const char*>(r.data + r.at), length);
  return r.Skip(length, end);
}
bool ReadString16(Reader& r, size_t end, std::u16string& result) {
  uint32_t count = 0;
  // Spore property strings: BIG-endian character count, UTF-16LE payload.
  if (!r.BE32(count, end) || count > kMaxStringUnits ||
      size_t(count) > r.Remaining(end) / 2) return false;
  result.clear();
  result.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    uint16_t c;
    if (!r.LE16(c, end)) return false;
    result.push_back(static_cast<char16_t>(c));
  }
  return true;
}
bool ReadKey(Reader& r, size_t end, ResourceKey& key) {
  return r.LE32(key.instance, end) &&
         r.LE32(key.type, end) &&
         r.LE32(key.group, end);
}
bool ReadFloatLE(Reader& r, size_t end, float& f) {
  uint32_t bits = 0;
  if (!r.LE32(bits, end)) return false;
  f = AsFloat(bits);
  return true;
}
bool ReadVec2(Reader& r, size_t end, Vector2& v) {
  return ReadFloatLE(r, end, v.x) && ReadFloatLE(r, end, v.y);
}
bool ReadVec3(Reader& r, size_t end, Vector3& v) {
  return ReadFloatLE(r, end, v.x) && ReadFloatLE(r, end, v.y) &&
         ReadFloatLE(r, end, v.z);
}
bool ReadVec4(Reader& r, size_t end, Vector4& v) {
  return ReadFloatLE(r, end, v.x) && ReadFloatLE(r, end, v.y) &&
         ReadFloatLE(r, end, v.z) && ReadFloatLE(r, end, v.w);
}
size_t ArrayWidth(uint16_t type) {
  switch (type) {
    case PropertyList::kBool: return 1;
    case PropertyList::kInt32:
    case PropertyList::kUInt32:
    case PropertyList::kFloat: return 4;
    case PropertyList::kKey:
    case PropertyList::kVector3:
    case PropertyList::kColorRGB: return 12;
    case PropertyList::kVector2: return 8;
    case PropertyList::kVector4:
    case PropertyList::kColorRGBA: return 16;
    case PropertyList::kBoundingBox: return 24;
    case PropertyList::kText: return 8;
    case PropertyList::kString8:
    case PropertyList::kString16: return 4; // minimum length prefix
    default: return 0;
  }
}

bool ReadOne(Reader& r, uint16_t type, size_t end,
             PropertyElement& result, bool isArray) {
  uint32_t x = 0;
  switch (type) {
    case PropertyList::kBool: {
      uint8_t v = 0;
      if (!r.Byte(v, end) || v > 1) return false;
      result = (v != 0);
      return true;
    }
    case PropertyList::kInt32:
      if (!r.BE32(x, end)) return false;
      result = AsInt32(x);
      return true;
    case PropertyList::kUInt32:
      if (!r.BE32(x, end)) return false;
      result = x;
      return true;
    case PropertyList::kFloat:
      if (!r.BE32(x, end)) return false;
      result = AsFloat(x);
      return true;
    case PropertyList::kString8: {
      std::string s;
      if (!ReadString8(r, end, s)) return false;
      result = std::move(s);
      return true;
    }
    case PropertyList::kString16: {
      std::u16string s;
      if (!ReadString16(r, end, s)) return false;
      result = std::move(s);
      return true;
    }
    case PropertyList::kKey: {
      ResourceKey key;
      if (!ReadKey(r, end, key) || (!isArray && !r.Skip(4, end))) return false;
      result = key;
      return true;
    }
    case PropertyList::kVector2: {
      Vector2 v;
      if (!ReadVec2(r, end, v) || (!isArray && !r.Skip(8, end))) return false;
      result = v;
      return true;
    }
    case PropertyList::kVector3:
    case PropertyList::kColorRGB: {
      Vector3 v;
      if (!ReadVec3(r, end, v) || (!isArray && !r.Skip(4, end))) return false;
      result = v;
      return true;
    }
    case PropertyList::kVector4:
    case PropertyList::kColorRGBA: {
      Vector4 v;
      if (!ReadVec4(r, end, v)) return false;
      result = v;
      return true;
    }
    case PropertyList::kBoundingBox: {
      BoundingBox v;
      if (!ReadVec3(r, end, v.minimum) || !ReadVec3(r, end, v.maximum))
        return false;
      result = v;
      return true;
    }
    default: return false;
  }
}

bool ReadProperty(Reader& r, Property& p) {
  const bool array = p.IsArray();
  if (!array) {
    PropertyElement item;
    if (!ReadOne(r, p.type, r.size, item, false)) return false;
    p.values.push_back(std::move(item));
    return true;
  }
  uint32_t count = 0, stride = 0;
  if (!r.BE32(count, r.size) || !r.BE32(stride, r.size) ||
      count > kMaxElements || stride > kMaxStride) return false;
  const size_t width = ArrayWidth(p.type);
  if (width == 0) return false;
  // Spore's string arrays use a nominal stride of 16, irrespective of
  // actual string lengths; read sequential length-prefixed items instead.
  const bool variable = p.type == PropertyList::kString8 ||
                        p.type == PropertyList::kString16;
  if (variable) {
    if (size_t(count) > r.Remaining(r.size) / 4) return false;
  } else {
    if ((count != 0 && stride < width) ||
        (stride != 0 && size_t(count) > r.Remaining(r.size) / stride))
      return false;
  }
  p.values.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    const size_t end = variable ? r.size : r.at + stride;
    PropertyElement value;
    if (p.type == PropertyList::kText) {
      // Localized text: LE table/instance plus fixed-size UTF-16LE,
      // NUL-padded placeholder. The length is supplied by the record stride.
      uint32_t table = 0, id = 0;
      if (!r.LE32(table, end) || !r.LE32(id, end) ||
          r.Remaining(end) % 2 != 0) return false;
      LocalizedText text;
      text.table = table;
      text.id = id;
      bool ended = false;
      while (r.at < end) {
        uint16_t unit = 0;
        if (!r.LE16(unit, end)) return false;
        if (unit == 0) ended = true;
        if (!ended) text.placeholder.push_back(static_cast<char16_t>(unit));
      }
      value = std::move(text);
    } else if (!ReadOne(r, p.type, end, value, true)) {
      return false;
    }
    if (!variable && !r.Skip(end - r.at, end)) return false;
    p.values.push_back(std::move(value));
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
      size_t(count) > r.Remaining(size) / 8) {
    error_ = "invalid property count";
    return false;
  }
  std::vector<Property> parsed;
  parsed.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    Property property;
    if (!r.BE32(property.id, size) || !r.BE16(property.type, size) ||
        !r.BE16(property.flags, size) || !ReadProperty(r, property)) {
      error_ = "unsupported or malformed property #" + std::to_string(i) +
               " at offset " + std::to_string(r.at);
      return false;
    }
    parsed.push_back(std::move(property));
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
