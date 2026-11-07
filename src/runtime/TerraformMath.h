#pragma once

#include <cstdint>

namespace Spore::Terrain {

// Portable C++17 extraction of two gameplay functions migrated from
// src/game/SPTerrain/SPTerrainTerraform.cpp, original executable VAs:
//
// GetTerraformPlanetType(float) at 0x00FC1FD0.
// FUN_00FC2040(float, float) at 0x00FC2040.
//
// This wrapper does NOT claim byte-for-byte x87 equivalence: the reference
// function uses MSVC x87 fsqrt, while this portable runtime uses std::sqrt.
// Compare against the user's owned game binary in a differential test later.
//
// The retail function reads a global kLavaThreshold; pass it explicitly so
// the runtime can be deterministic without recreating undocumented globals.
uint32_t PlanetTypeFromTerraformValue(float value, float lavaThreshold);
float TerraformBrushFalloff(float normalizedX, float normalizedY);

} // namespace Spore::Terrain
