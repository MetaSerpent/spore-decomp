#pragma once
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <vector>

namespace Spore::Creature {

// New portable creature-prototyping code for BlackSpore. The original
// Spore creature editor, skeleton and animation code have NOT been recovered.
struct Traits {
  float bodySize=0.5f, locomotion=0.5f, defense=0.5f, sensing=0.5f;
  uint32_t colour=0x00E4AD71U; // 0xRRGGBB
};
struct Vec3 {float x=0,y=0,z=0;};
struct Triangle {uint32_t a=0,b=0,c=0,colour=0;};
struct Mesh {
  std::vector<Vec3> vertices;
  std::vector<Triangle> triangles;
};
// Local coordinates: Y-up, forward toward negative Z; feet near Y=0.
// Phase is radians, normalized modulo a full gait cycle. Morphology and
// animation are derived entirely from traits, requiring no original assets.
// Bounded tessellation: 6 to 24 sectors, 3 to 16 latitude subdivisions.
Mesh GenerateCreature(const Traits& traits,float phase,
                      unsigned sectors=12,unsigned latitude=6);
void WriteCreatureOBJ(const Mesh& mesh,std::ostream& out);

// Continuous collision proxy for creature footprint (world units).
// Same body-size mapping is used in the generated creature morphology.
float FootprintRadius(float bodySize);

} // namespace Spore::Creature
