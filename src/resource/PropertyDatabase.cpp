#include "resource/PropertyDatabase.h"

#include <utility>

namespace Spore::Resource {

bool PropertyDatabase::Mount(const std::string& path, uint32_t priority) {
  if (!stack_.Mount(path, priority)) {
    error_ = stack_.Error();
    return false;
  }
  cache_.clear();
  ++generation_;
  error_.clear();
  return true;
}

void PropertyDatabase::Clear() {
  stack_.Clear();
  cache_.clear();
  hits_ = 0;
  misses_ = 0;
  ++generation_;
  error_.clear();
}

const ResolvedProperties* PropertyDatabase::Resolve(ResourceKey key,
                                                     size_t maxDepth) {
  key.type = kPropResourceType;
  if (maxDepth == 0 || maxDepth > 1024) {
    error_ = "maxDepth must be between 1 and 1024";
    return nullptr;
  }
  const CacheKey cacheKey{key, maxDepth};
  auto found = cache_.find(cacheKey);
  if (found != cache_.end()) {
    ++hits_;
    error_.clear();
    return &found->second;
  }
  ++misses_;
  ResolvedProperties resolved;
  if (!ResolvePropertyInheritance(stack_, key, resolved, error_, maxDepth))
    return nullptr;

  auto inserted = cache_.emplace(cacheKey, std::move(resolved));
  error_.clear();
  return &inserted.first->second;
}

const ResolvedProperty* PropertyDatabase::Find(ResourceKey key,
                                              uint32_t propertyId,
                                              size_t maxDepth) {
  const auto* props = Resolve(key, maxDepth);
  if (!props) return nullptr;
  const auto* result = props->Find(propertyId);
  if (!result) {
    error_ = "property ID not found in effective configuration";
    return nullptr;
  }
  error_.clear();
  return result;
}

} // namespace Spore::Resource
