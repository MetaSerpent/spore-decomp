#include "runtime/SoftwareView.h"
#include "runtime/TerraformField.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Spore::Terrain;
using namespace Spore::Terrain::View;
namespace {
void Check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
template<typename F> void Throws(F f,const char* msg){
 bool raised=false;try{f();}catch(const std::invalid_argument&){raised=true;}
 Check(raised,msg);
}
void TestRendering(){
  TerraformField terrain(24,16,.55f);
  const Camera camera;
  const auto first=RenderTerrain(terrain,camera,192,128);
  const auto again=RenderTerrain(terrain,camera,192,128);
  Check(first.width==192&&first.height==128&&first.rgb.size()==192*128,
        "framebuffer dimensions");
  Check(first.rgb==again.rgb&&first.Checksum()==again.Checksum(),
        "software rendering must be repeatable");
  Check(first.rgb[0]!=first.rgb[first.rgb.size()/2],
        "terrain appears over the background");
  std::ostringstream stream(std::ios::binary);
  WritePPM(first,stream);
  const auto serialized=stream.str();
  const auto magic=std::string("P6\n192 128\n255\n");
  Check(serialized.substr(0,magic.size())==magic,"PPM signature");
  Check(serialized.size()==magic.size()+192*128*3,"PPM exact byte count");
  terrain.Apply(.5f,.5f,.35f,.4f);
  const auto edited=RenderTerrain(terrain,camera,192,128);
  Check(edited.Checksum()!=first.Checksum(),"terraforming visually modifies scene");
  std::vector<Marker> markers={{.5f,.5f,0x00FFE070,5.f},
                                {.25f,.35f,0x00FF3020,4.f}};
  const auto populated=RenderTerrain(terrain,camera,192,128,markers);
  Check(populated.Checksum()!=edited.Checksum(),"creatures appear in scene");
  const auto populatedAgain=RenderTerrain(terrain,camera,192,128,markers);
  Check(populatedAgain.Checksum()==populated.Checksum(),"creature raster deterministic");
}
void TestCamera(){
  TerraformField terrain(12,12,.4f);
  Camera view;
  const auto original=RenderTerrain(terrain,view,160,120).Checksum();
  ApplyCameraAction(view,Action::RotateLeft);
  Check(RenderTerrain(terrain,view,160,120).Checksum()!=original,"camera rotation");
  ApplyCameraAction(view,Action::Reset);
  Check(RenderTerrain(terrain,view,160,120).Checksum()==original,"camera reset");
  ApplyCameraAction(view,Action::ZoomIn,3.f);
  Check(RenderTerrain(terrain,view,160,120).Checksum()!=original,"zoom action");
  ApplyCameraAction(view,Action::ZoomOut,3.f);
  ApplyCameraAction(view,Action::Left);
  ApplyCameraAction(view,Action::Up);
  Check(RenderTerrain(terrain,view,160,120).Checksum()!=original,"camera panning");
  for(int i=0;i<100;++i){
    ApplyCameraAction(view,Action::TiltDown);
    ApplyCameraAction(view,Action::ZoomOut);
  }
  Check(view.pitch>=.25f&&view.zoom>=.25f,"camera limits");
  const auto defaultCamera=Camera{};
  const auto centre=RenderTerrain(terrain,defaultCamera,160,120);
  (void)centre;
  const float factor=std::min(160.f,120.f)*.4f*defaultCamera.zoom;
  const float projectedY=60.f-std::cos(defaultCamera.pitch)*(.5f*.65f)*factor;
  const auto hit=ScreenToTerrain(defaultCamera,160,120,80.f,projectedY,.5f);
  Check(std::abs(hit.first-.5f)<.0001f&&std::abs(hit.second-.5f)<.0001f,
        "picking maps projected world centre back to world centre");
  Throws([&]{(void)ScreenToTerrain(defaultCamera,0,120,80,60);},
         "invalid picker size");
  Throws([&]{ApplyCameraAction(view,Action::ZoomIn,-1);},
         "invalid camera command strength");
}
void TestSafety(){
  TerraformField terrain(2,2,.5f);
  Camera cam;
  Throws([&]{(void)RenderTerrain(terrain,cam,0,120);},"reject zero width");
  Throws([&]{(void)RenderTerrain(terrain,cam,20000,10000);},
         "reject excessive raster allocation");
  cam.zoom=0;
  Throws([&]{(void)RenderTerrain(terrain,cam,120,120);},"reject invalid camera");
  cam=Camera{};
  Throws([&]{(void)RenderTerrain(terrain,cam,120,120,
             {Marker{.5f,.5f,0xFFFFFF,-1.f}});},
         "reject invalid organism marker");
  Frame invalid;invalid.width=2;invalid.height=2;invalid.rgb.resize(1);
  std::ostringstream out;
  Throws([&]{WritePPM(invalid,out);},"reject malformed frame");
}
}
int main(){
 try{
  TestRendering();TestCamera();TestSafety();
  std::cout<<"Spore software terrain viewer tests passed\n";
  return 0;
 }catch(const std::exception& e){
  std::cerr<<"Software terrain viewer regression failed: "<<e.what()<<'\n';
  return 1;
 }
}
