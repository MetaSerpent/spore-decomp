#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Spore::Creature {

// Independent deterministic 2D creature-contact physics.
// Coordinates use terrain tile units; size/radius use generated morphology.
// The solver is a BlackSpore runtime component, not recovered retail physics.
struct Body {
  uint64_t id=0;
  float x=0,z=0;
  float radius=.25f;
};
struct ContactSummary {
  size_t pairs=0;               // overlapping pairs on first iteration
  size_t corrections=0;         // pair corrections over all passes
  float maxRemainingDepth=0;    // maximum residual overlap after final pass
};
// Resolve using a deterministic uniform-grid broad phase and symmetric
// position corrections. No STL iteration-order-dependent collision sequence.
// Validates every input before changing any body (transactional on errors).
// Inputs are clamped against the world footprint. Iterations 1..12.
ContactSummary ResolveContacts(std::vector<Body>& bodies,
                               float worldWidth,float worldHeight,
                               unsigned passes=6);

} // namespace Spore::Creature
