# First portable gameplay logic from the reconstructed Spore engine

Source provenance:
- src/game/SPTerrain/SPTerrainTerraform.cpp - reconstructed gameplay code.
- Planet type classification: retail executable VA 0x00FC1FD0.
- Terrain brush radial falloff: retail executable VA 0x00FC2040.

The portable C++17 implementation in src/runtime/TerraformMath.cpp retains
the branch order of SP::GetTerraformPlanetType and the float->double numerical
formula from the migrated terrain brush function. The original reads a global
kLavaThreshold; the portable API passes that threshold explicitly.

The original brush used MSVC x87 inline fsqrt. Our portable version uses
std::sqrt, so exact bit-for-bit equivalence is NOT claimed. Confirm parity
against the owned Steam executable in the future differential harness.

Build/run without Steam data:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target terraform_math_tests spore_terraform_probe
    ctest --test-dir build --output-on-failure
    ./build/spore_terraform_probe type 0.8 0.75
    ./build/spore_terraform_probe grid 32 > brush.csv

Regression coverage includes the recovered planet thresholds, NaN/Inf branch
behaviour, brush strength at centre/boundary, radial monotonicity and
position symmetry over 10,000 synthetic coordinates. No retail assets are used.

Purpose: incrementally turn actual migrated gameplay modules into runnable,
portable components that later supply the BlackSpore renderer/terrain editor.
