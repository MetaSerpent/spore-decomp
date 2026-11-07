// Standalone, asset-free interactive demonstration of procedural creature
// anatomy, gait, contact physics, and software 3D rendering.
#include "runtime/CreatureGeometry.h"
#include "runtime/CreatureDynamics.h"
#include "runtime/NativeWindow.h"
#include "runtime/SoftwareView.h"
#include "runtime/TerraformField.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {
using namespace Spore::Creature;
using namespace Spore::Terrain;
using namespace Spore::Terrain::View;
constexpr float pi=3.14159265358979323846f;

class Demo {
public:
  Demo() : terrain_(28,20,.51f) { UpdatePhysics(); }
  void Tick() {
    if(!playing_)return;
    phase_+=.15f;
    UpdatePhysics();
  }
  void Key(const InputEvent& input) {
    if(input.kind!=InputKind::KeyDown)return;
    switch(input.key){
      case ' ':playing_=!playing_;break;
      case 'M':meshes_=!meshes_;break;
      case 'A':ApplyCameraAction(camera_,Action::RotateLeft);break;
      case 'D':ApplyCameraAction(camera_,Action::RotateRight);break;
      case 'W':ApplyCameraAction(camera_,Action::TiltUp);break;
      case 'S':ApplyCameraAction(camera_,Action::TiltDown);break;
      case LeftArrow:ApplyCameraAction(camera_,Action::Left);break;
      case RightArrow:ApplyCameraAction(camera_,Action::Right);break;
      case UpArrow:ApplyCameraAction(camera_,Action::Up);break;
      case DownArrow:ApplyCameraAction(camera_,Action::Down);break;
      case '+':ApplyCameraAction(camera_,Action::ZoomIn);break;
      case '-':ApplyCameraAction(camera_,Action::ZoomOut);break;
      case 'R':ApplyCameraAction(camera_,Action::Reset);break;
      default:break;
    }
  }
  Frame Paint(uint32_t w,uint32_t h) {
    std::vector<CreatureInstance> creatures;
    std::vector<Marker> markers;
    creatures.reserve(8);
    for(unsigned i=0;i<8;++i) {
      const auto* body=physics_.Find(i+1);
      if(!body)throw std::runtime_error("missing creature motion state");
      Traits t=TraitsFor(i);
      const float x=body->x/27.f,z=body->z/19.f;
      if(meshes_){
        CreatureInstance instance;
        instance.x=x;instance.z=z;
        instance.traits=t;
        instance.phase=phase_*4.f+float(i)*.6f;
        instance.yaw=phase_*.35f+float(i)*(pi/4.f);
        creatures.push_back(instance);
      }else{
        markers.push_back({x,z,t.colour,5.f});
      }
    }
    return RenderTerrain(terrain_,camera_,w,h,markers,creatures);
  }
private:
  Traits TraitsFor(unsigned i) const {
    Traits t;
    t.bodySize=.22f+float(i)*.09f;
    t.locomotion=.25f+float(i%4)*.22f;
    t.defense=.18f+float(i%3)*.30f;
    t.sensing=.25f+float((i+1)%4)*.20f;
    t.colour=(uint32_t(105+i*18)<<16)|
             (uint32_t(215-i*10)<<8)|
             uint32_t(110+i*13);
    return t;
  }
  void UpdatePhysics(){
    std::vector<Body> targets;
    targets.reserve(8);
    for(unsigned i=0;i<8;++i){
      const float angle=float(i)*pi/4.f+phase_*.4f;
      targets.push_back({
        uint64_t(i+1),13.5f+3.4f*std::cos(angle),
        9.5f+3.4f*std::sin(angle),
        FootprintRadius(TraitsFor(i).bodySize)
      });
    }
    (void)physics_.Advance(targets,27.f,19.f);
  }
  TerraformField terrain_;
  MotionWorld physics_;
  Camera camera_;
  float phase_=0;
  bool playing_=true,meshes_=true;
};

void Usage(){
  std::cerr<<"Usage: spore_creature_live [--frames N] [--ppm capture.ppm]\n"
           <<"A/D rotate, W/S tilt, arrows pan, +/- zoom, R reset,"
           <<" Space pause, M toggle 3D/dots, Esc quit.\n";
}
unsigned ParseCount(const char* input){
  unsigned frames=0;
  const std::string s(input);
  const auto r=std::from_chars(s.data(),s.data()+s.size(),frames);
  if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size()||frames<1||frames>300)
    throw std::invalid_argument("frame count must be 1..300");
  return frames;
}
}
int main(int argc,char** argv){
 try{
  unsigned frameLimit=0;
  std::string ppmPath;
  for(int i=1;i<argc;++i){
    const std::string arg(argv[i]);
    if(arg=="--frames"&&i+1<argc)frameLimit=ParseCount(argv[++i]);
    else if(arg=="--ppm"&&i+1<argc)ppmPath=argv[++i];
    else{Usage();return 2;}
  }
  Demo demo;
  uint32_t rendered=0;
  Frame last;
  DesktopCallbacks callbacks;
  callbacks.onTick=[&]{demo.Tick();};
  callbacks.onFrame=[&](uint32_t w,uint32_t h){
    last=demo.Paint(w,h);
    ++rendered;
    return last;
  };
  callbacks.onInput=[&](const InputEvent& e){demo.Key(e);};
  callbacks.shouldClose=[&]{return frameLimit!=0&&rendered>=frameLimit;};
  std::cout<<"Spore creature reconstruction prototype: native 3D demo\n";
  const int code=RunDesktop("Spore creature geometry + gait prototype",960,600,callbacks);
  if(!ppmPath.empty()){
    std::ofstream out(ppmPath,std::ios::binary|std::ios::trunc);
    if(!out)throw std::runtime_error("cannot open screenshot output");
    WritePPM(last,out);
  }
  std::cout<<"frames="<<rendered<<" checksum="<<last.Checksum()<<'\n';
  return code;
 }catch(const std::exception& e){
  std::cerr<<"spore_creature_live: "<<e.what()<<'\n';return 1;
 }
}
