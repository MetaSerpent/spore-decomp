# Deterministic terrain navigation

The source reconstruction already contains procedural creature geometry,
continuous motion/contact resolution, animated gait and small recovered
CreatureAnim conversion functions. This module adds the missing next layer:
portable **deterministic A*** path planning for mobile agents.

It uses a fixed cardinal grid, considers an elevation-based minimum walkable
height, rejects steps whose elevation change exceeds a configurable slope, and
adds a quantized cost penalty to steep traversals. Its heuristic is Manhattan
distance, in integer thousandths, so it is admissible even when the slope
penalty is high. The open queue orders by (f,g,node-index) and updates
predecessors only on lower cost, yielding repeatable results.

Routing is stateless and operates on the terrain as it exists **at the time
of the call**. When terraforming changes the world, the caller can reconstruct
a NavigationGrid and replan. The grid is capped at 262144 samples to bound
CPU and memory; a route may return no nodes if disconnected.

`spore_creature_route <snapshot> sx sz gx gz [waterline] [maxStep] [slopeCost]`
outputs CSV waypoints. Result code 3 means unreachable (not a parse error).
Unit and subprocess tests use original-asset-free synthetic terrain data.

This is BlackSpore-owned pathfinding code, NOT a claim of having reconstructed
the original Spore AI, world navigation or pathfinding algorithms. Next,
a controller can apply waypoints to the existing per-creature MotionWorld and
player/AI goal state.

## Persistent player / AI destinations

`CreatureOrders` stores actor-ID-to-destination assignments in a canonical,
versioned snapshot and recomputes the next A* waypoint from the *current*
creature position at each update. That means terrain edits automatically
trigger replanning, and unavailable paths remain pending rather than
directing the actor through an obstacle. Player destinations take precedence
over the simulation-provided default motion targets; the existing collision
and velocity solver still enforces contact separation and inertia.

Unit tests check movement toward a goal, deterministic save/restore replay,
terrain detours, unreachable destinations and corrupted snapshots.
The standalone module does not alter the original Spore AI; it is a
new engine-facing behavior layer for the BlackSpore game runtime.
