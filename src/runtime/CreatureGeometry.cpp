#include "runtime/CreatureGeometry.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <ostream>
#include <stdexcept>

namespace Spore::Creature {
namespace {
constexpr float pi=3.14159265358979323846f;
bool finiteUnit(float value) {
  return std::isfinite(value)&&value>=0&&value<=1;
}
void appendEllipsoid(Mesh& mesh,Vec3 centre,Vec3 scale,uint32_t colour,
                     unsigned sectors,unsigned latitude) {
  const auto start=uint32_t(mesh.vertices.size());
  for(unsigned ring=0;ring<=latitude;++ring) {
    const auto polar=pi*float(ring)/float(latitude);
    const auto sp=std::sin(polar),cp=std::cos(polar);
    for(unsigned sector=0;sector<=sectors;++sector) {
      const auto angle=2*pi*float(sector)/float(sectors);
      mesh.vertices.push_back({
        centre.x+scale.x*sp*std::cos(angle),
        centre.y+scale.y*cp,
        centre.z+scale.z*sp*std::sin(angle)
      });
    }
  }
  const auto stride=sectors+1;
  for(unsigned ring=0;ring<latitude;++ring)
    for(unsigned sector=0;sector<sectors;++sector) {
      const auto a=start+ring*stride+sector;
      const auto b=a+stride;
      // Consistent CCW outward winding, with both pole degenerate faces
      // omitted instead of creating zero-area triangles.
      if(ring>0) mesh.triangles.push_back({a,a+1,b,colour});
      if(ring+1<latitude) mesh.triangles.push_back({a+1,b+1,b,colour});
    }
}

void validate(const Traits& t,float phase,unsigned sectors,unsigned latitude) {
  if(!finiteUnit(t.bodySize)||!finiteUnit(t.locomotion)||
     !finiteUnit(t.defense)||!finiteUnit(t.sensing)||
     !std::isfinite(phase)||sectors<6||sectors>24||
     latitude<3||latitude>16)
    throw std::invalid_argument("invalid creature traits, animation phase or tessellation");
}
} // namespace

float FootprintRadius(float bodySize) {
  if(!finiteUnit(bodySize))
    throw std::invalid_argument("body size outside [0,1]");
  return .16f+.29f*bodySize;
}

Mesh GenerateCreature(const Traits& t,float phase,
                      unsigned sectors,unsigned latitude) {
  validate(t,phase,sectors,latitude);
  // Hard stop on pathological build parameters, even if limits are widened.
  Mesh mesh;
  const float radius=FootprintRadius(t.bodySize);
  const float length=.35f+.55f*t.bodySize;
  const float height=.26f+.28f*t.bodySize;
  const float bodyCentre=height+.05f;
  const auto wave=std::sin(std::remainder(phase,2*pi));
  const uint32_t main=t.colour&0xFFFFFFU;
  const uint32_t pale=0x00F6E4CBU;
  const uint32_t dark=0x00352C35U;

  appendEllipsoid(mesh,{0,bodyCentre,0},{radius,height,length},main,
                  sectors,latitude);
  // Four legs with alternating (diagonal) gait, visibly dependent on phase.
  for(int side : {-1,1}) for(int end : {-1,1}) {
    const float swing=wave*float(side*end)*(.02f+.18f*t.locomotion);
    const float footRise=std::max(0.f,swing)*(.05f+.13f*t.locomotion);
    appendEllipsoid(mesh,{
        float(side)*radius*.70f,.10f+footRise,
        float(end)*length*.65f+swing
      },{
        .07f+.04f*t.bodySize,.14f,.10f
      },main,sectors,latitude);
  }
  // Sensing enlarges two forward-facing eyes and widens their separation.
  const float eye=.065f+.070f*t.sensing;
  for(int side : {-1,1})
    appendEllipsoid(mesh,{
      float(side)*radius*.56f,bodyCentre+height*.32f,
      -length*.83f
    },{eye,eye,eye},pale,sectors,latitude);
  for(int side : {-1,1})
    appendEllipsoid(mesh,{
      float(side)*radius*.56f,bodyCentre+height*.32f,
      -length*.83f-eye*.80f
    },{eye*.38f,eye*.44f,eye*.22f},dark,sectors,latitude);
  // A tail reflects locomotion, while defensive spines reflect defense.
  appendEllipsoid(mesh,{
    0,bodyCentre+wave*.035f,length*.90f
  },{
    .045f+.03f*t.locomotion,.06f+.05f*t.locomotion,
    .13f+.13f*t.locomotion
  },main,sectors,latitude);
  if(t.defense>.34f) {
    const unsigned spikes=t.defense>.72f?3:2;
    for(unsigned i=0;i<spikes;++i) {
      const float z=-length*.32f+float(i)*length*.30f;
      const float tall=.10f+.24f*t.defense;
      appendEllipsoid(mesh,{0,bodyCentre+height*.9f+tall*.4f,z},
                      {.065f,tall*.6f,.08f},dark,sectors,latitude);
    }
  }
  return mesh;
}

void WriteCreatureOBJ(const Mesh& mesh,std::ostream& out) {
  if(mesh.vertices.size()>1000000||mesh.triangles.size()>2000000)
    throw std::invalid_argument("creature mesh too large");
  for(const auto& v:mesh.vertices)
    if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))
      throw std::invalid_argument("nonfinite creature vertex");
  for(const auto& triangle:mesh.triangles)
    if(triangle.a>=mesh.vertices.size()||
       triangle.b>=mesh.vertices.size()||
       triangle.c>=mesh.vertices.size())
      throw std::invalid_argument("invalid triangle vertex index");
  out<<"# BlackSpore procedural creature prototype (not original Spore assets)\n"
        "o BlackSporeCreature\n";
  for(const auto& v:mesh.vertices)
    out<<"v "<<v.x<<' '<<v.y<<' '<<v.z<<'\n';
  for(const auto& t:mesh.triangles)
    out<<"f "<<t.a+1<<' '<<t.b+1<<' '<<t.c+1<<'\n';
  if(!out)throw std::runtime_error("creature OBJ output failed");
}
} // namespace Spore::Creature
