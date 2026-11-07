// Asset-free creature morphology + gait generator for the BlackSpore runtime.
// Public fork produces OBJ meshes and deterministic physical contact probes.
#include "runtime/CreatureGeometry.h"
#include "runtime/CreaturePhysics.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
float Parse(const char* value){
  errno=0;
  char* end=nullptr;
  const auto number=std::strtof(value,&end);
  if(end==value||*end!='\0'||errno==ERANGE||!std::isfinite(number))
    throw std::invalid_argument("expected a finite float argument");
  return number;
}
void Help(){
 std::cerr<<"Usage:\n"
  " spore_creature_probe mesh <bodySize> <locomotion> <defense> <sensing>"
  " <phaseRadians> <output.obj>\n"
  " spore_creature_probe contact <x1> <z1> <radius1>"
  " <x2> <z2> <radius2>\n";
}
}
int main(int argc,char** argv){
 try{
  using namespace Spore::Creature;
  if(argc==8&&std::string(argv[1])=="mesh"){
    Traits traits;
    traits.bodySize=Parse(argv[2]);
    traits.locomotion=Parse(argv[3]);
    traits.defense=Parse(argv[4]);
    traits.sensing=Parse(argv[5]);
    const auto phase=Parse(argv[6]);
    const auto mesh=GenerateCreature(traits,phase);
    std::ofstream out(argv[7],std::ios::binary|std::ios::trunc);
    if(!out)throw std::runtime_error("cannot create creature OBJ file");
    WriteCreatureOBJ(mesh,out);
    std::cout<<"vertices="<<mesh.vertices.size()
             <<" triangles="<<mesh.triangles.size()<<'\n';
    return 0;
  }
  if(argc==8&&std::string(argv[1])=="contact"){
    const auto x1=Parse(argv[2]),z1=Parse(argv[3]),r1=Parse(argv[4]);
    const auto x2=Parse(argv[5]),z2=Parse(argv[6]),r2=Parse(argv[7]);
    std::vector<Body> objects{{1,x1,z1,r1},{2,x2,z2,r2}};
    const auto stats=ResolveContacts(objects,50,50);
    std::cout<<"pairs="<<stats.pairs
      <<" corrections="<<stats.corrections
      <<" remaining="<<stats.maxRemainingDepth
      <<" x1="<<objects[0].x<<" z1="<<objects[0].z
      <<" x2="<<objects[1].x<<" z2="<<objects[1].z<<'\n';
    return 0;
  }
  Help();
  return 2;
 }catch(const std::invalid_argument&e){
  std::cerr<<"spore_creature_probe: "<<e.what()<<'\n';return 2;
 }catch(const std::exception&e){
  std::cerr<<"spore_creature_probe: "<<e.what()<<'\n';return 1;
 }
}
