// No game data required. Reconstruct package containers to test layering semantics.
#include "resource/PackageStack.h"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace Spore::Resource;
namespace fs = std::filesystem;

namespace {
void Check(bool cond, const char* msg) {
  if (!cond) throw std::runtime_error(msg);
}
void Set32(std::vector<uint8_t>& v, size_t pos, uint32_t n) {
  if (v.size() < pos + 4) v.resize(pos + 4);
  for (unsigned k = 0; k < 4; ++k) v[pos + k] = uint8_t(n >> (k * 8));
}
void Push32(std::vector<uint8_t>& v, uint32_t n) { Set32(v, v.size(), n); }
std::vector<uint8_t> MakePackage(const std::vector<std::pair<ResourceKey, std::string>>& files) {
  std::vector<uint8_t> bytes(0x60, 0);
  bytes[0] = 'D'; bytes[1] = 'B'; bytes[2] = 'P'; bytes[3] = 'F';
  std::vector<uint8_t> index;
  Push32(index, 4); Push32(index, 0);
  for (const auto& f : files) {
    auto pos = static_cast<uint32_t>(bytes.size());
    bytes.insert(bytes.end(), f.second.begin(), f.second.end());
    Push32(index, f.first.type);
    Push32(index, f.first.group);
    Push32(index, f.first.instance);
    Push32(index, pos);
    Push32(index, static_cast<uint32_t>(f.second.size()));
    Push32(index, static_cast<uint32_t>(f.second.size()));
  }
  Set32(bytes, 4, 3);
  Set32(bytes, 0x24, static_cast<uint32_t>(files.size()));
  Set32(bytes, 0x2c, static_cast<uint32_t>(index.size()));
  Set32(bytes, 0x3c, 3);
  Set32(bytes, 0x40, static_cast<uint32_t>(bytes.size()));
  bytes.insert(bytes.end(), index.begin(), index.end());
  return bytes;
}
void Write(const fs::path& path,
           const std::vector<std::pair<ResourceKey, std::string>>& files) {
  auto bytes = MakePackage(files);
  std::ofstream stream(path, std::ios::binary);
  Check(static_cast<bool>(stream), "open package fixture");
  stream.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
  Check(static_cast<bool>(stream), "write package fixture");
}
std::string Text(PackageStack& stack, ResourceKey key) {
  auto bytes = stack.Read(key);
  Check(bytes.has_value(), "read resource");
  return std::string(bytes->begin(), bytes->end());
}
void Test() {
  const ResourceKey shared{0x10, 0x20, 0x30};
  const ResourceKey low_only{0x11, 0x20, 0x30};
  const ResourceKey high_only{0x12, 0x20, 0x30};
  const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  const auto dir = fs::temp_directory_path() / ("spore_stack_" + suffix);
  fs::create_directories(dir);
  const auto low = dir / "low.package";
  const auto high = dir / "high.package";
  const auto tie = dir / "tie.package";
  try {
    Write(low, {{shared, "low"}, {shared, "duplicate"}, {low_only, "local"}});
    Write(high, {{shared, "high"}, {high_only, "other"}});
    Write(tie, {{shared, "tie"}});
    PackageStack stack;
    Check(stack.Size() == 0 && stack.LayerCount() == 0, "initial empty");
    Check(!stack.Locate(shared), "absent key");
    Check(!stack.Mount((dir / "not-found.package").string(), 999), "failed mount");
    Check(!stack.Error().empty() && stack.LayerCount() == 0, "atomic mount failure");
    Check(stack.Mount(low.string(), 10), "mount low");
    Check(stack.Error().empty(), "clear error on successful mount");
    Check(stack.Size() == 2, "deduplicate in archive");
    Check(Text(stack, shared) == "low", "first-entry-wins");
    Check(stack.Mount(high.string(), 20), "mount high");
    Check(stack.Size() == 3, "overlay distinct key count");
    const auto ordered = stack.EffectiveEntries();
    Check(ordered.size() == 3, "effective index count");
    Check(ordered[0].key.instance == 0x10 &&
          ordered[1].key.instance == 0x11 &&
          ordered[2].key.instance == 0x12, "stable sorted resource index");
    Check(ordered[0].layer == 1, "effective index source layer");
    Check(Text(stack, shared) == "high", "higher priority overrides");
    Check(Text(stack, low_only) == "local", "retains lower unique key");
    Check(Text(stack, high_only) == "other", "high-only key");
    const auto selected = stack.Locate(shared);
    Check(selected && selected->layer == 1 && selected->entry.memSize == 4,
          "source location metadata");
    Check(stack.Mount(tie.string(), 20), "mount equal priority");
    Check(Text(stack, shared) == "tie", "later equal priority wins");
    Check(stack.Layers().size() == 3 && stack.Layers()[0].entries == 3,
          "layer metadata");
    Check(stack.Mount(low.string(), 5), "mount lower priority last");
    Check(Text(stack, shared) == "tie", "lower priority does not override");
    Check(!stack.Read(ResourceKey{0xFF, 0, 0}), "missing key returns no data");
    Check(!stack.Error().empty(), "missing-key error set");
    Check(Text(stack, low_only) == "local" && stack.Error().empty(), "read clears error");
    stack.Clear();
    Check(stack.Size() == 0 && stack.LayerCount() == 0, "clear stack");
    Check(!stack.Locate(shared), "no stale pointers after clear");
  } catch (...) {
    fs::remove_all(dir);
    throw;
  }
  fs::remove_all(dir);
}
}
int main() {
  try {
    Test();
    std::cout << "Spore package overlay tests: PASS\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Spore package overlay tests: FAIL " << e.what() << "\n";
    return 1;
  }
}
