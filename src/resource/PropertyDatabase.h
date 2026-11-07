#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "resource/PropertyResolver.h"

namespace Spore::Resource {

// Read-only, asset-driven configuration manager for a reconstructed runtime.
//
// Mounting or clearing packages invalidates all cached pointers. Cache entries
// remain stable across other Resolve() calls; callers must not retain pointers
// across successful Mount() or Clear(). Methods are NOT thread safe.
//
// Unlike a single .prop parser, this class retains package priority,
// inheritance and property provenance, so game systems can request effective
// configuration values repeatedly without re-reading and decoding assets.
class PropertyDatabase {
 public:
  // Successful mounts invalidate cached values. A failed mount keeps the old
  // package stack and caches usable, but returns a diagnostic through Error().
  bool Mount(const std::string& path, uint32_t priority = 0);
  void Clear();

  // Returns nullptr on missing/cyclic/corrupt resources. Requests are memoized
  // by both resource key and maxDepth, so smaller safety limits are respected
  // even after a previous successful deeper lookup.
  const ResolvedProperties* Resolve(ResourceKey key, size_t maxDepth = 64);

  const ResolvedProperty* Find(ResourceKey key, uint32_t propertyId,
                               size_t maxDepth = 64);

  // Only scalar values with exactly the requested variant type are returned.
  // A mismatched type is reported, not implicitly converted.
  template<typename T>
  const T* Get(ResourceKey key, uint32_t propertyId, size_t maxDepth = 64) {
    static_assert(!std::is_reference<T>::value, "T must be a value type");
    const auto* found = Find(key, propertyId, maxDepth);
    if (!found) return nullptr;
    if (found->value.IsArray() || found->value.values.size() != 1) {
      error_ = "property is an array or has no scalar value";
      return nullptr;
    }
    const T* result = std::get_if<T>(&found->value.values.front());
    if (!result) {
      error_ = "property type does not match the requested type";
      return nullptr;
    }
    error_.clear();
    return result;
  }

  const PackageStack& Packages() const { return stack_; }
  const std::string& Error() const { return error_; }
  size_t CacheEntries() const { return cache_.size(); }
  size_t CacheHits() const { return hits_; }
  size_t CacheMisses() const { return misses_; }
  size_t Generation() const { return generation_; }

 private:
  struct CacheKey {
    ResourceKey key;
    size_t maxDepth;
    bool operator==(const CacheKey& rhs) const {
      return key == rhs.key && maxDepth == rhs.maxDepth;
    }
  };
  struct CacheHash {
    size_t operator()(const CacheKey& v) const {
      const size_t h = ResourceKeyHash{}(v.key);
      return h ^ (size_t(v.key.type) * size_t(0x9e3779b1U)) ^
             (v.maxDepth + size_t(0x9e3779b9U) + (h << 6) + (h >> 2));
    }
  };
  PackageStack stack_;
  std::unordered_map<CacheKey, ResolvedProperties, CacheHash> cache_;
  std::string error_;
  size_t hits_ = 0;
  size_t misses_ = 0;
  size_t generation_ = 0;
};

} // namespace Spore::Resource
