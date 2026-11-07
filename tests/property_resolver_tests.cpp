#include "resource/PropertyResolver.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <variant>

using namespace Spore::Resource;
namespace fs = std::filesystem;

namespace {
using Bytes = std::vector<uint8_t>;
struct Record { ResourceKey key; Bytes payload; };
void Check(bool c, const char* label) {
  if (!c) throw std::runtime_error(label);
}
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
void Put32(Bytes& b, size_t at, uint32_t x) {
  if (b.size() < at + 4) b.resize(at + 4);
  for (unsigned i = 0; i < 4; ++i) b[at + i] = uint8_t(x >> (8 * i));
}
void AddProp(Bytes& b, uint32_t id, uint16_t type, const Bytes& payload) {
  BE32(b, id); BE16(b, type); BE16(b, 0);
  b.insert(b.end(), payload.begin(), payload.end());
}
Bytes KeyPayload(uint32_t group, uint32_t instance) {
  Bytes b;
  LE32(b, instance); LE32(b, 0); LE32(b, group); LE32(b, 0);
  return b;
}
Bytes BEValue(uint32_t n) {
  Bytes b; BE32(b, n); return b;
}
Bytes MakeProp(const std::vector<std::pair<uint32_t, Bytes>>& values) {
  // Already-encoded property records; count is big-endian.
  Bytes result; BE32(result, uint32_t(values.size()));
  for (const auto& e : values)
    result.insert(result.end(), e.second.begin(), e.second.end());
  return result;
}
Bytes RecordValue(uint32_t id, uint16_t type, const Bytes& bytes) {
  Bytes result;
  AddProp(result, id, type, bytes);
  return result;
}
ResourceKey Key(uint32_t instance) {
  return ResourceKey{instance, kPropResourceType, 0x100};
}
void WritePackage(const fs::path& path, const std::vector<Record>& records) {
  Bytes pkg(0x60, 0), index;
  pkg[0] = 'D'; pkg[1] = 'B'; pkg[2] = 'P'; pkg[3] = 'F';
  Put32(pkg, 4, 3); Put32(pkg, 0x24, uint32_t(records.size()));
  Put32(pkg, 0x3c, 3);
  LE32(index, 4); LE32(index, 0); // third key field declared constant 0
  for (const auto& r : records) {
    uint32_t offset = uint32_t(pkg.size());
    pkg.insert(pkg.end(), r.payload.begin(), r.payload.end());
    LE32(index, r.key.type);
    LE32(index, r.key.group);
    LE32(index, r.key.instance);
    LE32(index, offset);
    LE32(index, uint32_t(r.payload.size()));
    LE32(index, uint32_t(r.payload.size()));
  }
  Put32(pkg, 0x40, uint32_t(pkg.size()));
  Put32(pkg, 0x2c, uint32_t(index.size()));
  pkg.insert(pkg.end(), index.begin(), index.end());
  std::ofstream out(path, std::ios::binary);
  Check(bool(out), "write synthetic DBPF fixture");
  out.write(reinterpret_cast<const char*>(pkg.data()),
            static_cast<std::streamsize>(pkg.size()));
  Check(bool(out), "finish writing fixture");
}

void TestInheritance() {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path dir = fs::temp_directory_path() /
      ("spore_property_resolver_" + std::to_string(stamp));
  fs::create_directories(dir);
  try {
    Bytes grand = MakeProp({
        {10, RecordValue(10, PropertyList::kInt32, BEValue(7))},
        {11, RecordValue(11, PropertyList::kFloat, BEValue(0x3FC00000))},
    });
    Bytes parent = MakeProp({
        {kParentPropertyID, RecordValue(kParentPropertyID, PropertyList::kKey,
                                         KeyPayload(0x100, 1))},
        {10, RecordValue(10, PropertyList::kInt32, BEValue(8))},
        {12, RecordValue(12, PropertyList::kBool, Bytes{1})},
    });
    Bytes child = MakeProp({
        {kParentPropertyID, RecordValue(kParentPropertyID, PropertyList::kKey,
                                         KeyPayload(0x100, 2))},
        {11, RecordValue(11, PropertyList::kFloat, BEValue(0x40600000))},
        {11, RecordValue(11, PropertyList::kFloat, BEValue(0x42C60000))},
    });
    Bytes patch = MakeProp({
        {kParentPropertyID, RecordValue(kParentPropertyID, PropertyList::kKey,
                                         KeyPayload(0x100, 2))},
        {11, RecordValue(11, PropertyList::kFloat, BEValue(0x40900000))},
    });
    Bytes missing = MakeProp({
        {kParentPropertyID, RecordValue(kParentPropertyID, PropertyList::kKey,
                                         KeyPayload(0x100, 999))}
    });
    Bytes cycleA = MakeProp({
        {kParentPropertyID, RecordValue(kParentPropertyID, PropertyList::kKey,
                                         KeyPayload(0x100, 41))}
    });
    Bytes cycleB = MakeProp({
        {kParentPropertyID, RecordValue(kParentPropertyID, PropertyList::kKey,
                                         KeyPayload(0x100, 40))}
    });

    const fs::path base = dir / "base.package";
    const fs::path patchPath = dir / "patch.package";
    WritePackage(base, {{Key(1), grand}, {Key(2), parent}, {Key(3), child},
                        {Key(30), missing}, {Key(40), cycleA}, {Key(41), cycleB}});
    WritePackage(patchPath, {{Key(3), patch}});
    PackageStack stack;
    Check(stack.Mount(base.string()), "mount base");
    ResolvedProperties resolved;
    std::string error;
    Check(ResolvePropertyInheritance(stack, Key(3), resolved, error), "resolve ancestry");
    Check(resolved.ancestry.size() == 3 && resolved.ancestry[0].instance == 1 &&
          resolved.ancestry[1].instance == 2 && resolved.ancestry[2].instance == 3,
          "ancestry ordered oldest to child");
    Check(std::get<int32_t>(resolved.Find(10)->value.values[0]) == 8,
          "parent overrides grandparent");
    Check(std::get<float>(resolved.Find(11)->value.values[0]) == 3.5f,
          "child value wins and first duplicate wins");
    Check(std::get<bool>(resolved.Find(12)->value.values[0]),
          "grandchild inherits inherited boolean");
    Check(resolved.Find(10)->definedIn.instance == 2 &&
          resolved.Find(11)->definedIn.instance == 3,
          "property definition provenance");

    Check(stack.Mount(patchPath.string()), "mount last-wins patch");
    Check(ResolvePropertyInheritance(stack, Key(3), resolved, error), "resolve patched child");
    Check(std::get<float>(resolved.Find(11)->value.values[0]) == 4.5f &&
          resolved.Find(11)->packageLayer == 1, "patch provenance and priority");
    Check(std::get<int32_t>(resolved.Find(10)->value.values[0]) == 8,
          "patch still inherits parent definitions");

    Check(!ResolvePropertyInheritance(stack, Key(30), resolved, error) &&
          resolved.values.empty() && !error.empty(), "missing ancestor fails atomically");
    Check(!ResolvePropertyInheritance(stack, Key(40), resolved, error) &&
          error.find("cyclic") != std::string::npos, "detect cyclic inheritance");
    Check(!ResolvePropertyInheritance(stack, Key(3), resolved, error, 2) &&
          resolved.values.empty(), "enforce maximum depth");
    Check(!ResolvePropertyInheritance(stack, Key(3), resolved, error, 0),
          "reject invalid max depth");
  } catch (...) {
    fs::remove_all(dir);
    throw;
  }
  fs::remove_all(dir);
}
} // namespace

int main() {
  try {
    TestInheritance();
    std::cout << "Spore .prop inheritance/overrides regression tests passed.\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Property resolver test failure: " << e.what() << '\n';
    return 1;
  }
}
