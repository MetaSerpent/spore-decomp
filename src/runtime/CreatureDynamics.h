#pragma once
#include "runtime/CreaturePhysics.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Spore::Creature {

// Persistent kinematic creature motion. A gameplay engine supplies its desired
// target positions after AI/locomotion decisions. This independent runtime then
// smooths movement and corrects physical overlaps before rendering/interactions.
// New BlackSpore code; not recovered Spore animation/physics routines.
struct MotionBody {
  uint64_t id=0;
  float x=0,z=0,velocityX=0,velocityZ=0,radius=.25f;
};
class MotionWorld {
 public:
  // Strong failure guarantee: on any validation failure, state is unchanged.
  // Width and height are world-unit max coordinates, each >=2.
  ContactSummary Advance(const std::vector<Body>& targets,
                         float worldWidth,float worldHeight,
                         float response=.35f);
  const std::vector<MotionBody>& Bodies() const noexcept { return bodies_; }
  const MotionBody* Find(uint64_t id) const noexcept;
  uint64_t Steps() const noexcept { return steps_; }
  uint64_t Checksum() const noexcept;
  std::string Snapshot() const;
  static MotionWorld Restore(std::string_view snapshot);

 private:
  uint64_t steps_=0;
  // Always sorted by increasing non-zero ID to make replay and serialization
  // invariant to input actor ordering or STL hash map iteration.
  std::vector<MotionBody> bodies_;
};

} // namespace Spore::Creature
