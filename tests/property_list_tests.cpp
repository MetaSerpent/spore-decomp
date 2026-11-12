#include "resource/PropertyList.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using Spore::Resource::PropertyList;
using Spore::Resource::ResourceKey;
using Spore::Resource::Vector3;

namespace {
void Check(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}
using Bytes = std::vector<uint8_t>;
void BE16(Bytes& b, uint16_t x) {
  b.push_back(uint8_t(x >> 8)); b.push_back(uint8_t(x));
}
void BE32(Bytes& b, uint32_t x) {
  b.push_back(uint8_t(x >> 24)); b.push_back(uint8_t(x >> 16));
  b.push_back(uint8_t(x >> 8)); b.push_back(uint8_t(x));
}
void LE32(Bytes& b, uint32_t x) {
  b.push_back(uint8_t(x)); b.push_back(uint8_t(x >> 8));
  b.push_back(uint8_t(x >> 16)); b.push_back(uint8_t(x >> 24));
}
void Float(Bytes& b, float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  LE32(b, bits);
}
void String(Bytes& b, const std::u16string& value) {
  BE32(b, uint32_t(value.size()));
  for (char16_t c : value) {
    b.push_back(uint8_t(c));
    b.push_back(uint8_t(c >> 8));
  }
}
void Key(Bytes& b, uint32_t instance, uint32_t type, uint32_t group) {
  LE32(b, instance); LE32(b, type); LE32(b, group);
}
void Vec3(Bytes& b, float x, float y, float z) {
  Float(b, x); Float(b, y); Float(b, z);
}
void Record(Bytes& b, uint32_t id, uint16_t type, uint16_t flags,
            const Bytes& payload) {
  BE32(b, id); BE16(b, type); BE16(b, flags);
  b.insert(b.end(), payload.begin(), payload.end());
}
Bytes MixedFixture() {
  Bytes data;
  BE32(data, 3);
  Bytes key;
  Key(key, 0xAA, 0xBB, 0xCC); LE32(key, 0xDEADBEEF); // padding
  Record(data, 1, PropertyList::kKey, 0, key);
  Bytes vector;
  Vec3(vector, 1.5f, -2.0f, 3.25f); LE32(vector, 0);
  Record(data, 2, PropertyList::kVector3, 0, vector);
  Bytes text;
  String(text, u"Spore \u00e9\U0001F600");
  Record(data, 3, PropertyList::kString16, 0, text);
  return data;
}
void TestScalars() {
  PropertyList props;
  const Bytes input = MixedFixture();
  Check(props.Parse(input), "parse scalar property file");
  Check(props.Properties().size() == 3, "count parsed properties");
  const auto* a = props.Find(1);
  const auto* b = props.Find(2);
  const auto* c = props.Find(3);
  Check(a && b && c && !props.Find(4), "lookup by hashed id");
  auto k = std::get<ResourceKey>(a->values[0]);
  Check(k.instance == 0xAA && k.type == 0xBB && k.group == 0xCC, "resource key field order");
  auto v = std::get<Vector3>(b->values[0]);
  Check(v.x == 1.5f && v.y == -2.0f && v.z == 3.25f, "vector float endian");
  Check(std::get<std::u16string>(c->values[0]) == u"Spore \u00e9\U0001F600", "UTF-16LE string");
  Check(!a->IsArray(), "scalar flags");
}
void TestArrays() {
  PropertyList props;
  Bytes input; BE32(input, 3);
  Bytes keys;
  BE32(keys, 2); BE32(keys, 12);
  Key(keys, 1, 2, 3); Key(keys, 4, 5, 6);
  Record(input, 10, PropertyList::kKey, 0x30, keys);
  Bytes vectors;
  BE32(vectors, 2); BE32(vectors, 16);
  Vec3(vectors, -1, 0, 2); LE32(vectors, 0xFACE);
  Vec3(vectors, 3, 4, 5); LE32(vectors, 0xBABE);
  Record(input, 11, PropertyList::kVector3, 0x10, vectors);
  Bytes strings;
  BE32(strings, 2); BE32(strings, 16); // nominal stride is inaccurate for strings
  String(strings, u"one"); String(strings, u"two");
  Record(input, 12, PropertyList::kString16, 0x20, strings);
  Check(props.Parse(input), "parse known prop arrays");
  const auto& ks = props.Find(10)->values;
  const auto& vs = props.Find(11)->values;
  const auto& ss = props.Find(12)->values;
  Check(ks.size() == 2 && std::get<ResourceKey>(ks[1]).instance == 4 && std::get<ResourceKey>(ks[1]).group == 6, "12-byte key arrays");
  Check(vs.size() == 2 && std::get<Vector3>(vs[1]).z == 5.0f, "16-byte padded vector arrays");
  Check(ss.size() == 2 && std::get<std::u16string>(ss[0]) == u"one" && std::get<std::u16string>(ss[1]) == u"two",
        "variable-size string arrays");
}
void TestSafety() {
  PropertyList props;
  const Bytes good = MixedFixture();
  Check(props.Parse(good), "sanity fixture");
  for (size_t n = 0; n < good.size(); ++n) {
    Bytes prefix(good.begin(), good.begin() + n);
    Check(!props.Parse(prefix), "reject all truncated prefixes");
    Check(props.Properties().empty() && !props.Error().empty(), "failure clears stale state");
  }
  Check(props.Parse(good), "can reparse after failed inputs");
  Bytes extra = good;
  extra.push_back(0);
  Check(!props.Parse(extra), "reject trailing bytes");
  Bytes unknown; BE32(unknown, 1);
  Record(unknown, 7, 0x9999, 0, {});
  Check(!props.Parse(unknown), "unknown scalar type rejected");
  Bytes bad_count = {0xFF, 0xFF, 0xFF, 0xFF};
  Check(!props.Parse(bad_count), "reject malicious property count");
  Bytes bad_array; BE32(bad_array, 1);
  Bytes payload;
  BE32(payload, 0xFFFFFFFF); BE32(payload, 12);
  Record(bad_array, 8, PropertyList::kKey, 0x30, payload);
  Check(!props.Parse(bad_array), "reject oversized array");
  Bytes bad_size; BE32(bad_size, 1);
  Bytes short_key;
  BE32(short_key, 1); BE32(short_key, 8);
  LE32(short_key, 1); LE32(short_key, 2);
  Record(bad_size, 9, PropertyList::kKey, 0x10, short_key);
  Check(!props.Parse(bad_size), "reject malformed key stride");
  Bytes bad_text; BE32(bad_text, 1);
  Bytes text;
  LE32(text, 0xFFFFFFFF);
  Record(bad_text, 9, PropertyList::kString16, 0, text);
  Check(!props.Parse(bad_text), "reject oversized text length");
  Bytes valid_empty; BE32(valid_empty, 0);
  Check(props.Parse(valid_empty) && props.Properties().empty(), "empty property list");
}

void TestCommonTypes() {
  PropertyList props;
  Bytes input; BE32(input, 7);
  Record(input, 20, PropertyList::kBool, 0, Bytes{1});
  Bytes integer; BE32(integer, 0xFFFFFFFE);
  Record(input, 21, PropertyList::kInt32, 0, integer);
  Bytes unsignedInt; BE32(unsignedInt, 0x87654321);
  Record(input, 22, PropertyList::kUInt32, 0, unsignedInt);
  Bytes real; BE32(real, 0x3FC00000); // BE float 1.5
  Record(input, 23, PropertyList::kFloat, 0, real);
  Bytes text8; BE32(text8, 4);
  text8.insert(text8.end(), {'c', 'e', 'l', 'l'});
  Record(input, 24, PropertyList::kString8, 0, text8);
  Bytes vec; Float(vec, -2); Float(vec, 3); LE32(vec, 0); LE32(vec, 0);
  Record(input, 25, PropertyList::kVector2, 0, vec);
  Bytes color; Vec3(color, 0.25f, 0.5f, 0.75f); Float(color, 1);
  Record(input, 26, PropertyList::kColorRGBA, 0, color);
  Check(props.Parse(input), "parse representative game tuning types");
  Check(std::get<bool>(props.Find(20)->values[0]), "bool");
  Check(std::get<int32_t>(props.Find(21)->values[0]) == -2, "int32 BE signed");
  Check(std::get<uint32_t>(props.Find(22)->values[0]) == 0x87654321, "uint32 BE unsigned");
  Check(std::get<float>(props.Find(23)->values[0]) == 1.5f, "float BE");
  Check(std::get<std::string>(props.Find(24)->values[0]) == "cell", "string8 BE length");
  auto pos = std::get<Spore::Resource::Vector2>(props.Find(25)->values[0]);
  Check(pos.x == -2.0f && pos.y == 3.0f, "vector2 LE with scalar padding");
  auto rgba = std::get<Spore::Resource::Vector4>(props.Find(26)->values[0]);
  Check(rgba.x == 0.25f && rgba.w == 1.0f, "RGBA LE");
}

void TestLocalizedTextAndBoundingBox() {
  PropertyList props;
  Bytes input; BE32(input, 2);
  Bytes text;
  BE32(text, 1); BE32(text, 16);
  LE32(text, 0xAABBCCDD); LE32(text, 0x17);
  text.insert(text.end(), {'O', 0, 'K', 0, 0, 0, 0, 0});
  Record(input, 40, PropertyList::kText, 0x1c, text);
  Bytes bbox;
  BE32(bbox, 1); BE32(bbox, 24);
  Vec3(bbox, -1, -2, -3);
  Vec3(bbox, 1, 2, 3);
  Record(input, 41, PropertyList::kBoundingBox, 0, bbox);
  Check(props.Parse(input), "localized text and bbox parsed");
  auto localized = std::get<Spore::Resource::LocalizedText>(props.Find(40)->values[0]);
  Check(localized.table == 0xAABBCCDD && localized.id == 0x17 &&
        localized.placeholder == u"OK", "localized placeholder UTF-16LE");
  auto box = std::get<Spore::Resource::BoundingBox>(props.Find(41)->values[0]);
  Check(box.minimum.x == -1.0f && box.maximum.z == 3.0f, "bbox min and max");
}

void TestPropertyArraysAndEmpty() {
  PropertyList props;
  Bytes input; BE32(input, 3);
  Bytes booleans; BE32(booleans, 3); BE32(booleans, 1);
  booleans.insert(booleans.end(), {0, 1, 0});
  Record(input, 55, PropertyList::kBool, 0x9c, booleans);
  Bytes floats; BE32(floats, 2); BE32(floats, 4);
  BE32(floats, 0x40000000); BE32(floats, 0x40400000);
  Record(input, 56, PropertyList::kFloat, 0x9c, floats);
  Bytes empty; BE32(empty, 0); BE32(empty, 12);
  Record(input, 57, PropertyList::kKey, 0x9c, empty);
  Check(props.Parse(input), "array of bools, floats and empty keys");
  const auto& b = props.Find(55)->values;
  const auto& f = props.Find(56)->values;
  Check(b.size() == 3 && !std::get<bool>(b[0]) && std::get<bool>(b[1]),
        "unpacked boolean array");
  Check(f.size() == 2 && std::get<float>(f[0]) == 2.0f &&
        std::get<float>(f[1]) == 3.0f, "big endian float array");
  Check(props.Find(57)->values.empty(), "zero-length array is valid");
}
void TestTransforms() {
  // Generated values only; no installed game bytes in this fixture.
  Bytes input; BE32(input, 2);
  Bytes transforms; BE32(transforms, 2); BE32(transforms, 60);
  for (unsigned i = 0; i < 2; ++i) {
    transforms.insert(transforms.end(), {0x23, 0x81, 0x56, 0x34});
    Vec3(transforms, float(i)+1.5f, -2.25f, 7.0f);
    Float(transforms, float(i)+.5f);
    for (unsigned j = 0; j < 9; ++j) Float(transforms, float(i*10+j)-3.5f);
    LE32(transforms, 0xDEADBEEF); // permitted per-item stride padding
  }
  Record(input, 70, PropertyList::kTransform, 0x9c, transforms);
  Bytes number; BE32(number, 12345);
  Record(input, 71, PropertyList::kUInt32, 0, number);
  PropertyList props;
  Check(props.Parse(input), "padded transform arrays parse through the following property");
  const auto& values = props.Find(70)->values;
  Check(values.size() == 2 && props.Find(70)->IsArray(), "transform count retained");
  const auto& a = std::get<Spore::Resource::Transform>(values[0]);
  const auto& b = std::get<Spore::Resource::Transform>(values[1]);
  Check(a.flags == 0x8123 && a.modificationCount == 0x3456, "transform u16 fields are little endian and preserved");
  Check(a.translation.x == 1.5f && a.translation.y == -2.25f && b.translation.x == 2.5f &&
        a.scale == .5f && b.scale == 1.5f, "transform translation and scale are little endian");
  Check(a.rotation[0] == -3.5f && a.rotation[8] == 4.5f && b.rotation[4] == 10.5f,
        "all nine asymmetric matrix entries retain serialized order");
  Check(std::get<uint32_t>(props.Find(71)->values[0]) == 12345, "item padding does not consume a following record");
  for (size_t n = 0; n < input.size(); ++n) {
    Check(!props.Parse(Bytes(input.begin(), input.begin()+n)), "truncated transforms fail safely");
    Check(props.Properties().empty(), "a truncated matrix clears previous parse state");
  }
  Bytes invalid; BE32(invalid, 1);
  Bytes shortItem; BE32(shortItem, 1); BE32(shortItem, 55); shortItem.resize(8+55, 0);
  Record(invalid, 72, PropertyList::kTransform, 0x10, shortItem);
  Check(!props.Parse(invalid), "transform strides smaller than 56 are rejected");
  Bytes empty; BE32(empty, 1);
  Bytes emptyItems; BE32(emptyItems, 0); BE32(emptyItems, 56);
  Record(empty, 73, PropertyList::kTransform, 0, emptyItems);
  Check(props.Parse(empty) && props.Find(73)->IsArray() && props.Find(73)->values.empty(),
        "transform type retains its implicit array form with empty contents");
}
} // namespace

int main() {
  try {
    TestScalars();
    TestArrays();
    TestCommonTypes();
    TestLocalizedTextAndBoundingBox();
    TestPropertyArraysAndEmpty();
    TestSafety();
    TestTransforms();
    std::cout << "Spore PropertyList decoder tests passed.\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "PropertyList test failure: " << e.what() << '\n';
    return 1;
  }
}
