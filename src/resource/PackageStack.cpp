#include "resource/PackageStack.h"
#include <utility>
#include <algorithm>
#include <tuple>

namespace Spore::Resource {

bool PackageStack::Mount(const std::string& path, uint32_t priority) {
  auto package = std::make_unique<DatabasePackedFile>();
  if (!package->Open(path)) {
    error_ = package->Error();
    return false;  // Existing layers remain usable after failed mount.
  }
  const size_t layer = layers_.size();
  const size_t n = package->Entries().size();
  layers_.push_back({{path, priority, n}, std::move(package)});
  const auto& entries = layers_.back().package->Entries();
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto& e = entries[i];
    // DBPF uses first-entry-wins for duplicates inside the same archive.
    if (layers_.back().package->Find(e.key) != &e)
      continue;
    auto it = resolved_.find(e.key);
    if (it == resolved_.end() || it->second.priority <= priority)
      resolved_[e.key] = Choice{layer, i, priority};
  }
  error_.clear();
  return true;
}

void PackageStack::Clear() {
  resolved_.clear();
  layers_.clear();
  error_.clear();
}

std::vector<PackageStack::Layer> PackageStack::Layers() const {
  std::vector<Layer> output;
  output.reserve(layers_.size());
  for (const auto& layer : layers_) output.push_back(layer.info);
  return output;
}

std::optional<PackageStack::Location> PackageStack::Locate(const ResourceKey& key) const {
  auto it = resolved_.find(key);
  if (it == resolved_.end()) return std::nullopt;
  const Choice& c = it->second;
  return Location{key, c.layer, layers_[c.layer].package->Entries()[c.entry]};
}

std::vector<PackageStack::Location> PackageStack::EffectiveEntries() const {
  std::vector<Location> output;
  output.reserve(resolved_.size());
  for (const auto& choice : resolved_) {
    output.push_back(Location{
        choice.first, choice.second.layer,
        layers_[choice.second.layer].package->Entries()[choice.second.entry]});
  }
  std::sort(output.begin(), output.end(), [](const Location& a, const Location& b) {
    return std::tie(a.key.type, a.key.group, a.key.instance) <
           std::tie(b.key.type, b.key.group, b.key.instance);
  });
  return output;
}

std::optional<std::vector<uint8_t>> PackageStack::Read(const ResourceKey& key) {
  auto it = resolved_.find(key);
  if (it == resolved_.end()) {
    error_ = "resource key not found";
    return std::nullopt;
  }
  const Choice& c = it->second;
  auto bytes = layers_[c.layer].package->Read(layers_[c.layer].package->Entries()[c.entry]);
  if (!bytes) {
    error_ = "resource read/decompression failed: " + layers_[c.layer].info.path;
    return std::nullopt;
  }
  error_.clear();
  return bytes;
}

} // namespace Spore::Resource
