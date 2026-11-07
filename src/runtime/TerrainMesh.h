#pragma once

#include "runtime/TerraformField.h"

#include <cstddef>
#include <cstdint>
#include <iosfwd>

namespace Spore::Terrain {

// Portable, asset-free offline mesh generation. Independent of retail Spore's
// unknown planet renderer. Generates conventional right-handed, Y-up OBJ.
struct TerrainMeshStats {
  std::size_t vertices = 0;
  std::size_t triangles = 0;
  float minimumHeight = 0;
  float maximumHeight = 0;
};

// Throws invalid_argument before writing on bad scale or excessive resolution.
// Unit-square X/Z coordinates; Y = normalized terrain * verticalScale.
// Vertex normals use central/one-sided derivatives; positive Y faces are CCW.
// A separate UV and normal record is written for each vertex.
TerrainMeshStats WriteTerrainOBJ(const TerraformField& terrain,
                                 std::ostream& output,
                                 float horizontalScale = 1.0f,
                                 float verticalScale = 1.0f);

} // namespace Spore::Terrain
