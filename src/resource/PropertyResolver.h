#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "resource/PackageStack.h"
#include "resource/PropertyList.h"

namespace Spore::Resource {

// Property lists are often templates: the "parent" key (0x00B2CCCB)
// points at another .prop in the resource manager. Resolve ancestry before
// using gameplay tuning so child values override inherited defaults.
constexpr uint32_t kPropResourceType = 0x00B1B104;
constexpr uint32_t kParentPropertyID = 0x00B2CCCB;

struct ResolvedProperty {
  Property value;
  ResourceKey definedIn;
  size_t packageLayer = 0;
};
struct ResolvedProperties {
  // Sorted to make comparisons and snapshots reproducible.
  std::map<uint32_t, ResolvedProperty> values;
  // Root ancestry order: oldest ancestor first, requested child last.
  std::vector<ResourceKey> ancestry;
  const ResolvedProperty* Find(uint32_t id) const {
    auto it = values.find(id);
    return it == values.end() ? nullptr : &it->second;
  }
};

// Resolves from mounted packages without modifying any files. Parent references
// are resolved as .prop regardless of the key's serialized "type" field; the
// retail property manager indexes by instance and group.
// Failure is transactional: output is empty and error describes the cause.
bool ResolvePropertyInheritance(PackageStack& stack, ResourceKey requested,
                                ResolvedProperties& output, std::string& error,
                                size_t maxDepth = 64);

} // namespace Spore::Resource
