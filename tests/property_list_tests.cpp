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
  LE32(b, uint32_t(value.size()));
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
  auto k = std::get<ResourceKey>(a->value);
  Check(k.instance == 0xAA && k.type == 0xBB && k.group == 0xCC, "resource key field order");
  auto v = std::get<Vector3>(b->value);
  Check(v.x == 1.5f && v.y == -2.0f && v.z == 3.25f, "vector float endian");
  Check(std::get<std::u16string>(c->value) == u"Spore \u00e9\U0001F600", "UTF-16LE string");
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
  BE32(strings, 2); BE32(strings, 0); // variable-size string array
  String(strings, u"one"); String(strings, u"two");
  Record(input, 12, PropertyList::kString16, 0x20, strings);
  Check(props.Parse(input), "parse known prop arrays");
  const auto ks = std::get<std::vector<ResourceKey>>(props.Find(10)->value);
  const auto vs = std::get<std::vector<Vector3>>(props.Find(11)->value);
  const auto ss = std::get<std::vector<std::u16string>>(props.Find(12)->value);
  Check(ks.size() == 2 && ks[1].instance == 4 && ks[1].group == 6, "12-byte key arrays");
  Check(vs.size() == 2 && vs[1].z == 5.0f, "16-byte padded vector arrays");
  Check(ss.size() == 2 && ss[0] == u"one" && ss[1] == u"two",
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
} // namespace

int main() {
  try {
    TestScalars();
    TestArrays();
    TestSafety();
    std::cout << "Spore PropertyList decoder tests passed.\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "PropertyList test failure: " << e.what() << '\n';
    return 1;
  }
}
