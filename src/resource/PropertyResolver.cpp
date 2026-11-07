#include "resource/PropertyResolver.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <variant>

namespace Spore::Resource {
namespace {

bool ResolveOne(PackageStack& stack, ResourceKey key,
                ResolvedProperties& output, std::string& error,
                std::unordered_set<ResourceKey, ResourceKeyHash>& visited,
                size_t maxDepth) {
  if (visited.size() >= maxDepth) {
    error = "property parent chain exceeds maxDepth";
    return false;
  }
  if (!visited.insert(key).second) {
    error = "cyclic .prop parent chain";
    return false;
  }
  const auto location = stack.Locate(key);
  if (!location) {
    error = "property resource is missing from mounted packages";
    return false;
  }
  auto bytes = stack.Read(key);
  if (!bytes) {
    error = "cannot decompress .prop: " + stack.Error();
    return false;
  }
  PropertyList list;
  if (!list.Parse(*bytes)) {
    error = "cannot parse .prop: " + list.Error();
    return false;
  }

  const Property* parent = list.Find(kParentPropertyID);
  if (parent) {
    if (parent->IsArray() || parent->type != PropertyList::kKey ||
        parent->values.size() != 1 ||
        !std::holds_alternative<ResourceKey>(parent->values.front())) {
      error = "invalid parent property (expected one scalar resource key)";
      return false;
    }
    ResourceKey parentKey = std::get<ResourceKey>(parent->values.front());
    // This type may be serialized as zero in Maxis' property templates.
    parentKey.type = kPropResourceType;
    if (!ResolveOne(stack, parentKey, output, error, visited, maxDepth))
      return false;
  }

  std::unordered_set<uint32_t> locallySeen;
  for (const Property& p : list.Properties()) {
    // Preserve first-entry-wins within a file; child keys override ancestors.
    if (locallySeen.insert(p.id).second)
      output.values[p.id] = ResolvedProperty{p, key, location->layer};
  }
  output.ancestry.push_back(key);
  return true;
}
} // namespace

bool ResolvePropertyInheritance(PackageStack& stack, ResourceKey requested,
                                ResolvedProperties& output, std::string& error,
                                size_t maxDepth) {
  output = {};
  error.clear();
  if (maxDepth == 0 || maxDepth > 1024) {
    error = "maxDepth must be between 1 and 1024";
    return false;
  }
  requested.type = kPropResourceType;
  ResolvedProperties candidate;
  std::unordered_set<ResourceKey, ResourceKeyHash> visited;
  if (!ResolveOne(stack, requested, candidate, error, visited, maxDepth))
    return false;
  output = std::move(candidate);
  return true;
}

} // namespace Spore::Resource
