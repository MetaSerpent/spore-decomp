#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "resource/DBPF.h"

namespace Spore::Resource {

// A read-only resource overlay, providing deterministic package priority.
// Higher priority wins; for equal priorities, the most recently mounted package
// wins. The FIRST occurrence of a key in one package wins, matching DBPF::Find.
// This models an essential part of Spore-style package patching without relying
// on retail executable code or bundling game assets.
class PackageStack {
 public:
  struct Location {
    ResourceKey key;
    size_t layer = 0;
    IndexEntry entry;
  };
  struct Layer {
    std::string path;
    uint32_t priority = 0;
    size_t entries = 0;
  };
  bool Mount(const std::string& path, uint32_t priority = 0);
  void Clear();
  size_t Size() const { return resolved_.size(); }
  size_t LayerCount() const { return layers_.size(); }
  std::vector<Layer> Layers() const;
  std::optional<Location> Locate(const ResourceKey& key) const;
  std::optional<std::vector<uint8_t>> Read(const ResourceKey& key);
  const std::string& Error() const { return error_; }

 private:
  struct OpenLayer {
    Layer info;
    std::unique_ptr<DatabasePackedFile> package;
  };
  struct Choice {
    size_t layer;
    size_t entry;
    uint32_t priority;
  };
  std::vector<OpenLayer> layers_;
  std::unordered_map<ResourceKey, Choice, ResourceKeyHash> resolved_;
  std::string error_;
};

} // namespace Spore::Resource
