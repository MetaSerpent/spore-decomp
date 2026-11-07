#include "resource/PropertyDatabase.h"

#include <filesystem>
#include <system_error>
#include <utility>

namespace Spore::Resource {

bool PropertyDatabase::MakeStamp(const std::string& path,
                                 uint32_t priority,
                                 MountSpec& result, std::string& error) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path absolute = fs::absolute(fs::path(path), ec);
  if (ec) {
    error = "cannot resolve package path: " + ec.message();
    return false;
  }
  const auto bytes = fs::file_size(absolute, ec);
  if (ec) {
    error = "cannot stat package file: " + absolute.string() + ": " + ec.message();
    return false;
  }
  const auto changed = fs::last_write_time(absolute, ec);
  if (ec) {
    error = "cannot read package timestamp: " + absolute.string() + ": " + ec.message();
    return false;
  }
  result = MountSpec{absolute.string(), priority, bytes, changed};
  return true;
}

bool PropertyDatabase::Mount(const std::string& path, uint32_t priority) {
  MountSpec spec;
  if (!MakeStamp(path, priority, spec, error_)) return false;
  if (!stack_.Mount(spec.path, priority)) {
    error_ = stack_.Error();
    return false;
  }
  mounts_.push_back(std::move(spec));
  cache_.clear();
  ++generation_;
  error_.clear();
  return true;
}

bool PropertyDatabase::Reload() {
  if (mounts_.empty()) {
    error_ = "cannot reload an empty package database";
    return false;
  }
  PackageStack candidate;
  std::vector<MountSpec> stamps;
  stamps.reserve(mounts_.size());
  for (const auto& mounted : mounts_) {
    MountSpec stamp;
    if (!MakeStamp(mounted.path, mounted.priority, stamp, error_))
      return false;
    if (!candidate.Mount(stamp.path, stamp.priority)) {
      error_ = "cannot reload package: " + stamp.path + ": " + candidate.Error();
      return false;
    }
    stamps.push_back(std::move(stamp));
  }
  stack_ = std::move(candidate);
  mounts_ = std::move(stamps);
  cache_.clear();
  ++generation_;
  error_.clear();
  return true;
}

bool PropertyDatabase::RefreshIfChanged() {
  for (const auto& mounted : mounts_) {
    MountSpec now;
    if (!MakeStamp(mounted.path, mounted.priority, now, error_))
      return Reload();
    if (now.byteSize != mounted.byteSize ||
        now.modified != mounted.modified)
      return Reload();
  }
  error_.clear();
  return true;
}

void PropertyDatabase::Clear() {
  stack_.Clear();
  mounts_.clear();
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
