# spore-decomp

A clean-room-style behavioral reimplementation of Spore's `SporeApp.exe`, built for
portability. Every reimplemented function is annotated with the original address it
mirrors (`@ 0x00XXXXXX`) and verified against the original code by differential tests.

**This repository contains no game code or data.** The binary-derived analysis
artifacts live in `work/` (git-ignored), are produced locally from your own copy of the
game, and must not be distributed.

## Layout
- `src/` — portable C++17 reimplementation (currently `resource/`: DBPF, RefPack, name hash)
- `tests/` — unit tests, plus corpus tests over your installed `Data/` when `SPORE_DATA` is set
- `tools/steamstub_analysis_image.py` — makes a non-runnable, decrypted analysis image
- `tools/ghidra/` — PyGhidra scripts (discovery, naming, decompile, xrefs)
- `tools/difftest/` — Unicorn harness that calls original functions by address
- `symbols/names.txt` — recovered names with provenance
- `docs/` — plan and file-format notes

## Procedural creature geometry and deterministic contacts

The new `spore_creature_runtime` C++17 library builds trait-dependent
procedural creature meshes (body, four articulated-looking animated feet,
eyes, tail and defensive spines) and supplies a **deterministic spatial-hash
contact solver** for circular collision proxies. This is original BlackSpore
engine development, **not a recovered Maxis creature editor/physics engine**.

```sh
cmake --build build --target spore_creature_probe creature_runtime_tests
./build/spore_creature_probe mesh .8 .9 .8 .7 1.6 creature.obj
./build/spore_creature_probe contact 3 3 .4 3.2 3 .4
```

The zero-dependency CPU viewer can now rasterize actual indexed creature
meshes with shading, depth occlusion against terrain and gait animation,
alongside its original terrain raster and optional marker overlays. Rendering
is bounded to 128 creatures per frame. `creature_view_tests` compares
repeatable frame hashes as traits, poses and positions change.

A separate `MotionWorld` layer now provides continuous, persistent
creature movement: stable ID-sorted actor state, damped velocities,
deterministic spatial contact resolution, dead-actor pruning and
versioned save/restore. It accepts gameplay movement targets but does not
require the original Spore runtime or any assets. Its test suite checks
150-step replay, shuffled actor order, overlapping spawns, invalid data and
snapshot reconstruction.

CI runs generated-geometry topology, gait, invalid-input and contact
regressions, plus executable-level OBJ exports without retail game data.

## Portable terrain state

The fork now includes an independently running, asset-free **headless terrain
field** built on the reconstructed Spore `TerraformBrushFalloff` and
`PlanetTypeFromTerraformValue` functions. Its grid, patch operations,
serialization, checksums and CLI are newly written runtime abstractions:
this is not yet the complete retail planet simulation.

```sh
cmake -S . -B build && cmake --build build --target spore_terrain_field terraform_field_tests
ctest --test-dir build --output-on-failure
./build/spore_terrain_field run 33 33 100 42 0.20 0.35 terrain.snapshot
./build/spore_terrain_field inspect terrain.snapshot
```

Windows/Linux CI, deterministic replay, snapshot corruption and sanitizer
tests run without original game assets. See `docs/TERRAIN_FIELD_MILESTONE.md`.

### Offline OBJ mesh export

The reconstructed terrain state can now be meshed into a conventional 3D
Wavefront OBJ with correctly wound triangles, Y-up normals, and UV coordinates.
This is new BlackSpore-compatible mesh-generation logic, **not a recovered
retail rendering implementation**.

```sh
./build/spore_terrain_field run 32 32 100 42 .25 .5 terrain.snapshot
./build/spore_terrain_mesh terrain.snapshot terrain.obj 100 30
```

The export is bounded to 262,144 vertices and has standalone C++ and
subprocess regression tests that do not require any Spore game assets.

### Native interactive terrain viewer

`spore_terrain_live` now opens an actual **native software-rendered window**
on Windows (Win32/GDI) and Linux/X11. No SDL, external game assets or GPU
are needed. It uses the portable reconstructed terrain subsystem and the
software 3D renderer. Camera controls: A/D rotate, W/S tilt, arrows pan,
+/- zoom, R reset; left/right click raises/lowers the terrain; F5 writes
an edited terrain snapshot. Escape closes the window.

```sh
cmake --build build --target spore_terrain_live
./build/spore_terrain_field run 48 32 50 42 .25 .2 terrain.snapshot
./build/spore_terrain_live terrain.snapshot
```

The public CI compiles the native application on Windows and Linux.
The Linux CI also launches the actual window and draws frames under
`xvfb-run`, so that even the native graphics-loop smoke test requires
no person or physical display. Windows is compile-tested; real desktop
interaction still warrants a later manual acceptance check. The
window itself still needs a desktop/display for an interactive smoke test.

## Setup
```sh
python3 -m venv .venv && .venv/bin/pip install pefile capstone pycryptodome unicorn
.venv/bin/python tools/steamstub_analysis_image.py "<Spore>/SporeBin/SporeApp.exe" \
    work/SporeApp.analysis.bin work/SporeApp.analysis.json
cmake -S . -B build -G Ninja && cmake --build build
SPORE_DATA="<Spore>/Data" ctest --test-dir build --output-on-failure
```
Ghidra project setup (about 20 min): see `docs/PLAN.md`.

## License
Released under the **GNU General Public License, version 2** (GPL-2.0-only) — see [`LICENSE`](LICENSE).
Third-party components and the status of binary-derived artifacts are described in
[`THIRD_PARTY.md`](THIRD_PARTY.md).
