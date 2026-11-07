#include "resource/PropertyDatabase.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Spore::Resource;
namespace fs = std::filesystem;

namespace {
using Bytes = std::vector<uint8_t>;
struct Entry { ResourceKey key; Bytes data; };

void Require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
void BE16(Bytes& v, uint16_t value) {
  v.push_back(uint8_t(value >> 8)); v.push_back(uint8_t(value));
}
void BE32(Bytes& v, uint32_t value) {
  v.push_back(uint8_t(value >> 24)); v.push_back(uint8_t(value >> 16));
  v.push_back(uint8_t(value >> 8)); v.push_back(uint8_t(value));
}
void LE32(Bytes& v, uint32_t value) {
  v.push_back(uint8_t(value)); v.push_back(uint8_t(value >> 8));
  v.push_back(uint8_t(value >> 16)); v.push_back(uint8_t(value >> 24));
}
void Put32(Bytes& v, size_t at, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) v.at(at + i) = uint8_t(value >> (i * 8));
}
Bytes PropertyRecord(uint32_t id, uint16_t type, uint16_t flags, Bytes payload) {
  Bytes record;
  BE32(record, id); BE16(record, type); BE16(record, flags);
  record.insert(record.end(), payload.begin(), payload.end());
  return record;
}
Bytes PropertyFile(std::initializer_list<Bytes> records) {
  Bytes bytes;
  BE32(bytes, uint32_t(records.size()));
  for (const auto& r : records) bytes.insert(bytes.end(), r.begin(), r.end());
  return bytes;
}
Bytes Parent(uint32_t group, uint32_t instance) {
  Bytes key;
  LE32(key, instance); LE32(key, 0); LE32(key, group); LE32(key, 0);
  return PropertyRecord(kParentPropertyID, PropertyList::kKey, 0, std::move(key));
}
Bytes Int(uint32_t id, uint32_t value) {
  Bytes v; BE32(v, value);
  return PropertyRecord(id, PropertyList::kInt32, 0, std::move(v));
}
Bytes Float(uint32_t id, uint32_t bits) {
  Bytes v; BE32(v, bits);
  return PropertyRecord(id, PropertyList::kFloat, 0, std::move(v));
}
Bytes Bool(uint32_t id, bool value) {
  return PropertyRecord(id, PropertyList::kBool, 0, Bytes{uint8_t(value)});
}
Bytes BoolArray(uint32_t id) {
  Bytes body; BE32(body, 2); BE32(body, 1);
  body.push_back(1); body.push_back(0);
  return PropertyRecord(id, PropertyList::kBool, 0x1c, std::move(body));
}
ResourceKey Key(uint32_t instance) {
  return ResourceKey{instance, kPropResourceType, 0x1200};
}
void WritePackage(const fs::path& path, const std::vector<Entry>& entries) {
  Bytes body(0x60, 0), index;
  body[0] = 'D'; body[1] = 'B'; body[2] = 'P'; body[3] = 'F';
  Put32(body, 4, 3);
  Put32(body, 0x24, uint32_t(entries.size()));
  Put32(body, 0x3C, 3);
  LE32(index, 4); LE32(index, 0);
  for (const auto& e : entries) {
    const uint32_t off = uint32_t(body.size());
    body.insert(body.end(), e.data.begin(), e.data.end());
    LE32(index, e.key.type); LE32(index, e.key.group);
    LE32(index, e.key.instance); LE32(index, off);
    LE32(index, uint32_t(e.data.size())); LE32(index, uint32_t(e.data.size()));
  }
  Put32(body, 0x40, uint32_t(body.size()));
  Put32(body, 0x2C, uint32_t(index.size()));
  body.insert(body.end(), index.begin(), index.end());
  std::ofstream stream(path, std::ios::binary);
  Require(bool(stream), "can create temporary package");
  stream.write(reinterpret_cast<const char*>(body.data()),
               static_cast<std::streamsize>(body.size()));
  Require(bool(stream), "can write temporary package");
}
struct TempDirectory {
  fs::path path;
  TempDirectory() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    path = fs::temp_directory_path() / ("spore_prop_db_" + std::to_string(stamp));
    fs::create_directories(path);
  }
  ~TempDirectory() { std::error_code error; fs::remove_all(path, error); }
};
void TestCachingAndOverrides() {
  TempDirectory tmp;
  const auto base = tmp.path / "base.package";
  const auto patch = tmp.path / "patch.package";
  const auto low = tmp.path / "low.package";
  const auto replacement = tmp.path / "replacement.package";
  WritePackage(base, {
      {Key(1), PropertyFile({Int(100, 17), Bool(101, true)})},
      {Key(2), PropertyFile({Parent(0x1200, 1), Float(102, 0x3FC00000),
                             BoolArray(103)})},
      {Key(3), PropertyFile({Parent(0x1200, 4)})},
      {Key(4), PropertyFile({Parent(0x1200, 3)})},
      {Key(5), PropertyFile({Parent(0x1200, 99)})},
      {Key(6), PropertyFile({PropertyRecord(100, 0xFFFF, 0, {})})},
  });
  WritePackage(patch, {
      {Key(2), PropertyFile({Parent(0x1200, 1), Float(102, 0x40200000)})}
  });
  WritePackage(low, {
      {Key(2), PropertyFile({Parent(0x1200, 1), Float(102, 0x40E00000)})}
  });
  WritePackage(replacement, {
      {Key(2), PropertyFile({Parent(0x1200, 1), Float(102, 0x40400000)})}
  });

  PropertyDatabase db;
  Require(db.Generation() == 0 && db.CacheEntries() == 0, "initial state");
  Require(db.Mount(base.string(), 1), "mount base package");
  Require(db.Packages().LayerCount() == 1, "access mounted layer metadata");
  Require(db.CacheEntries() == 0 && db.Generation() == 1, "mount generation");

  const auto* cfg = db.Resolve(Key(2));
  Require(cfg && cfg->ancestry.size() == 2, "resolves inheritance");
  Require(db.CacheMisses() == 1 && db.CacheEntries() == 1, "first decode caches result");
  Require(cfg->Find(100) && cfg->Find(100)->definedIn.instance == 1,
          "inherited provenance is preserved");
  Require(db.Resolve(Key(2)) == cfg && db.CacheHits() == 1, "cache hit reuses parsed data");
  Require(db.Get<int32_t>(Key(2), 100) && *db.Get<int32_t>(Key(2), 100) == 17,
          "typed inherited value");
  Require(db.Get<float>(Key(2), 102) && *db.Get<float>(Key(2), 102) == 1.5f,
          "typed local value");
  Require(db.Get<bool>(Key(2), 101) && *db.Get<bool>(Key(2), 101),
          "inherited boolean");
  Require(!db.Get<int32_t>(Key(2), 102) &&
          db.Error().find("type") != std::string::npos,
          "type mismatch fails without coercion");
  Require(!db.Get<bool>(Key(2), 103) &&
          db.Error().find("array") != std::string::npos,
          "array refused for scalar typed getter");
  Require(!db.Find(Key(2), 0xBAD) && !db.Error().empty(), "missing ID diagnostic");
  Require(db.Get<int32_t>(Key(2), 100) && db.Error().empty(),
          "successful call clears an earlier error");

  Require(!db.Resolve(Key(2), 1) && db.Error().find("maxDepth") != std::string::npos,
          "small depth limit is not bypassed by cache");
  Require(db.Resolve(Key(2), 2), "minimum sufficient depth");
  Require(db.CacheEntries() == 2, "cache key includes depth limit");
  Require(!db.Resolve(Key(3)) && db.Error().find("cyclic") != std::string::npos,
          "cycles are rejected");
  Require(!db.Resolve(Key(5)) && !db.Error().empty(), "missing parent detected");
  Require(!db.Resolve(Key(6)) && !db.Error().empty(), "malformed property detected");

  const auto before = db.Generation();
  const auto hits = db.CacheHits();
  Require(!db.Mount((tmp.path / "missing.package").string(), 99),
          "failed mount returns error");
  Require(db.Generation() == before && db.CacheEntries() == 2,
          "failed mount preserves cache and generation");
  Require(db.Resolve(Key(2), 2) && db.CacheHits() > hits,
          "failed mount does not invalidate previously valid config");

  Require(db.Mount(patch.string(), 3), "mount high-priority patch");
  Require(db.CacheEntries() == 0 && db.Generation() == before + 1,
          "patch invalidates config cache");
  Require(db.Get<float>(Key(2), 102) && *db.Get<float>(Key(2), 102) == 2.5f,
          "patch overrides local float");
  Require(db.Find(Key(2), 102)->packageLayer == 1, "patch provenance points to layer");
  Require(db.Get<int32_t>(Key(2), 100) && *db.Get<int32_t>(Key(2), 100) == 17,
          "patch still inherits grandparent value");
  Require(db.Mount(low.string(), 2), "mount later lower-priority package");
  Require(db.Get<float>(Key(2), 102) && *db.Get<float>(Key(2), 102) == 2.5f,
          "priority prevents a later lower-priority override");
  Require(db.Mount(replacement.string(), 3), "mount equal-priority replacement");
  Require(db.Get<float>(Key(2), 102) && *db.Get<float>(Key(2), 102) == 3.0f,
          "latest equal-priority patch wins");

  const auto previousGeneration = db.Generation();
  db.Clear();
  Require(db.Generation() == previousGeneration + 1 &&
          db.CacheEntries() == 0 && db.Packages().LayerCount() == 0,
          "clear invalidates and unmounts");
  Require(db.CacheHits() == 0 && db.CacheMisses() == 0, "clear resets counters");
  Require(!db.Get<float>(Key(2), 102) && !db.Error().empty(),
          "unmounted resources cannot leak through old cache");
}
} // namespace

int main() {
  try {
    TestCachingAndOverrides();
    std::cout << "Cached reconstructed Spore property database tests passed.\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Property database test failed: " << e.what() << '\n';
    return 1;
  }
}
