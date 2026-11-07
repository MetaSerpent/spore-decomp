#include "runtime/SoftwareView.h"
#include "runtime/CreatureGeometry.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace Spore::Terrain;
using namespace Spore::Terrain::View;
namespace {
void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
template<class Fn> void reject(Fn fn,const char* msg){
 bool thrown=false;try{fn();}catch(const std::invalid_argument&){thrown=true;}
 require(thrown,msg);
}
void TestCreatureScene(){
 TerraformField terrain(11,11,.52f);
 Camera camera;
 const auto background=RenderTerrain(terrain,camera,480,360);
 CreatureInstance creature;
 creature.x=.5f;creature.z=.5f;
 creature.yaw=.4f;creature.phase=0.f;
 creature.traits={.92f,.93f,.81f,.75f,0x00E0AA62};
 const auto frame=RenderTerrain(terrain,camera,480,360,{}, {creature});
 const auto replay=RenderTerrain(terrain,camera,480,360,{}, {creature});
 require(frame.Checksum()==replay.Checksum(),"3D creature render deterministic");
 require(frame.Checksum()!=background.Checksum(),"3D creature visible in depth buffer");
 creature.phase=1.57f;
 const auto animated=RenderTerrain(terrain,camera,480,360,{}, {creature});
 require(animated.Checksum()!=frame.Checksum(),"gait visually changes creature pose");
 creature.traits.defense=.1f;
 const auto lessDefended=RenderTerrain(terrain,camera,480,360,{}, {creature});
 require(lessDefended.Checksum()!=animated.Checksum(),"defensive anatomy renders differently");
 creature.x=.68f;creature.z=.68f;
 const auto moved=RenderTerrain(terrain,camera,480,360,{}, {creature});
 require(moved.Checksum()!=lessDefended.Checksum(),"world position affects rendered creature");
 creature.x=.5f;creature.z=.5f;
 creature.phase=0.f;
 const std::vector<CreatureInstance> actors{creature,creature};
 const auto multiple=RenderTerrain(terrain,camera,480,360,{},actors);
 require(multiple.Checksum()==RenderTerrain(terrain,camera,480,360,{},actors).Checksum(),
         "overlapping meshes depth-test deterministically");
 reject([&] {creature.x=std::numeric_limits<float>::quiet_NaN();
             RenderTerrain(terrain,camera,480,360,{}, {creature});},
        "reject nonfinite creature transform");
 creature.x=.5f;creature.traits.bodySize=-1.f;
 reject([&] {RenderTerrain(terrain,camera,480,360,{}, {creature});},
        "reject malformed creature genome");
 reject([&] {RenderTerrain(terrain,camera,480,360,{},
                std::vector<CreatureInstance>(129));},
        "bound number of rendered procedural meshes");
}
}
int main(){
 try{TestCreatureScene();std::cout<<"Procedural creature 3D rendering tests passed\n";return 0;}
 catch(const std::exception& e){std::cerr<<"Creature raster failure: "<<e.what()<<'\n';return 1;}
}
