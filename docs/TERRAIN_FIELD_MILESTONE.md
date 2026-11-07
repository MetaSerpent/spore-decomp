# Independent terrain field milestone

The full reconstructed Spore executable is not linkable yet. This milestone
provides a headless terrain state that can run today without Steam, GOG,
Spore ModAPI, game packages or manual E2E tests.

**Recovered retail-derived functions**

- `PlanetTypeFromTerraformValue` maps the scalar to Spore's recovered type branches.
- `TerraformBrushFalloff` supplies the radial falloff from the recovered functions.
- `TerraformField` is a NEW engine component: brush scaling, field storage,
  patch accumulation, checksum, and serialization are newly implemented.
  No claim of exact retail equivalence is made for the new parts.

**Contract**

- Dimensions 2x2 through one million samples; samples remain in [0,1].
- Normalized brush coordinates and radius; signed deltas raise/lower terrain.
- Brush updates visit only the bounding rectangle and validate inputs first.
- Deterministic seeded CLI simulation; snapshot save/restore with versioning.
- A portable C++17 static library for integration into standalone runtimes.

**Automated validation**

- `terraform_field_tests`: bounds, symmetry, 3,000 deterministic brush updates,
  save/restore, corruption rejection and matching replay checksums.
- `test_terrain_field_cli.py`: subprocess replay, seed differences, snapshot
  integrity and argument rejection.
- Windows and Linux CMake/CTest, plus Linux AddressSanitizer/UBSan.

**Still missing**

Terrain mesh generation, visual rendering, resource-driven planet content,
object placement, collision/physics, full game rules and retail differential tests.
