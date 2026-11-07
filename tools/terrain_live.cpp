// Interactively inspect a terrain snapshot using the dependency-free
// native Win32/GDI or X11 software viewer.
#include "runtime/NativeWindow.h"
#include "runtime/SoftwareView.h"
#include "runtime/TerraformField.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
std::string Read(const std::string& path){
 std::error_code err;
 if(std::filesystem::file_size(path,err)>20000000||err)
   throw std::runtime_error("terrain snapshot missing or over 20 MB");
 std::ifstream in(path,std::ios::binary);
 if(!in)throw std::runtime_error("cannot open terrain snapshot");
 return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
}
int main(int argc,char** argv){
 if(argc!=2){
  std::cerr<<"Usage: spore_terrain_live <terrain.snapshot>\n"
           <<"A/D rotate, W/S tilt, arrows pan, +/- zoom, R reset\n"
           <<"Left/right click raises/lowers terrain, F5 save edited snapshot, Escape quits\n";
  return 2;
 }
 try{
  using namespace Spore::Terrain;
  using namespace Spore::Terrain::View;
  auto field=TerraformField::Restore(Read(argv[1]));
  Camera camera;
  uint32_t lastWidth=960,lastHeight=600;
  const std::string savePath=std::string(argv[1])+".edited.snapshot";
  DesktopCallbacks cb;
  cb.onFrame=[&](uint32_t w,uint32_t h){
    lastWidth=w;lastHeight=h;
    return RenderTerrain(field,camera,w,h);
  };
  cb.onInput=[&](const InputEvent& e){
    if(e.kind==InputKind::MouseDown){
      const auto coords=ScreenToTerrain(camera,lastWidth,lastHeight,
                                       float(e.x),float(e.y),float(field.Mean()));
      if(coords.first>=0.f&&coords.first<=1.f&&
         coords.second>=0.f&&coords.second<=1.f)
        field.Apply(coords.first,coords.second,.16f,
                    e.mouseButton==1?.16f:-.16f);
      return;
    }
    if(e.kind!=InputKind::KeyDown)return;
    switch(e.key){
    case 'A':ApplyCameraAction(camera,Action::RotateLeft);break;
    case 'D':ApplyCameraAction(camera,Action::RotateRight);break;
    case 'W':ApplyCameraAction(camera,Action::TiltUp);break;
    case 'S':ApplyCameraAction(camera,Action::TiltDown);break;
    case LeftArrow:ApplyCameraAction(camera,Action::Left);break;
    case RightArrow:ApplyCameraAction(camera,Action::Right);break;
    case UpArrow:ApplyCameraAction(camera,Action::Up);break;
    case DownArrow:ApplyCameraAction(camera,Action::Down);break;
    case '+':ApplyCameraAction(camera,Action::ZoomIn);break;
    case '-':ApplyCameraAction(camera,Action::ZoomOut);break;
    case 'R':ApplyCameraAction(camera,Action::Reset);break;
    case F5:{
      std::ofstream out(savePath,std::ios::binary);
      if(!out||!(out<<field.Snapshot()))
        throw std::runtime_error("cannot save edited terrain snapshot");
      std::cout<<"Saved "<<savePath<<'\n';
      break;
    }
    default:break;
    }
  };
  std::cout<<"Native terrain viewer. Mouse brushes modify only in-memory terrain."
           <<" F5 saves to "<<savePath<<'\n';
  return RunDesktop("Spore terrain reconstruction - interactive",960,600,cb);
 }catch(const std::exception& e){
  std::cerr<<"spore_terrain_live: "<<e.what()<<'\n';
  return 1;
 }
}
