#pragma once
#include "runtime/CreatureDynamics.h"
#include "runtime/CreatureNavigation.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Spore::Creature {

// Persistent player/AI destinations. Builds a fresh A* path from the actor's
// current tile each tick, so moving creatures automatically adapt to newly
// terraformed terrain. This is new BlackSpore behavior code, not recovered AI.
struct Destination { uint32_t x=0,z=0; };
struct OrderStep {
  size_t directed=0;   // targets replaced by navigation waypoints
  size_t completed=0;  // goals reached and removed
  size_t blocked=0;    // temporarily no route from current terrain
  size_t removed=0;    // actor no longer exists
};
class MoveOrders {
 public:
  // Throws on malformed actor ID / coordinates. If unreachable, returns false
  // and does not replace a previously issued order for that actor.
  bool Issue(uint64_t actor,Waypoint from,Waypoint destination,
             const NavigationGrid& terrain);
  bool Cancel(uint64_t actor) noexcept;
  const Destination* Find(uint64_t actor) const noexcept;
  size_t Size() const noexcept { return destinations_.size(); }
  // Mutates only targets for actors with active destinations; collision and
  // velocity integration remain the caller's responsibility via MotionWorld.
  // Recomputes route from actor's actual position every step for edit safety.
  OrderStep Apply(std::vector<Body>& targets,const MotionWorld& current,
                  const NavigationGrid& terrain);

  std::string Snapshot() const;
  static MoveOrders Restore(std::string_view bytes);
  uint64_t Checksum() const noexcept;

 private:
  std::map<uint64_t,Destination> destinations_;
};

} // namespace Spore::Creature
