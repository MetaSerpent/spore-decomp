#include "runtime/SoftwareView.h"
#include "runtime/TerraformField.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
float Real(const char* s){
 errno=0;char* end=nullptr;
 const float f=std::strtof(s,&end);
 if(end==s||*end!='\0'||errno!=0||!std::isfinite(f))
   throw std::invalid_argument("invalid camera parameter");
 return f;
}
std::string Load(const std::string& path){
 std::error_code err;
 if(std::filesystem::file_size(path,err)>20000000||err)
   throw std::runtime_error("terrain snapshot missing or over 20MB");
 std::ifstream in(path,std::ios::binary);
 if(!in)throw std::runtime_error("cannot open terrain snapshot");
 return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
}
int main(int argc,char** argv){
 try{
  if(argc!=3&&argc!=8){
   std::cerr<<"Usage: spore_terrain_view <terrain.snapshot> <output.ppm>"
                " [width height yaw pitch zoom]\n";
   return 2;
  }
  using namespace Spore::Terrain;
  View::Camera camera;
  unsigned w=960,h=600;
  if(argc==8){
   const auto wf=Real(argv[3]),hf=Real(argv[4]);
   if(wf<32||wf>1920||hf<32||hf>1080||
      std::floor(wf)!=wf||std::floor(hf)!=hf)
     throw std::invalid_argument("raster dimensions invalid");
   w=static_cast<unsigned>(wf);h=static_cast<unsigned>(hf);
   camera.yaw=Real(argv[5]);camera.pitch=Real(argv[6]);
   camera.zoom=Real(argv[7]);
  }
  const auto field=TerraformField::Restore(Load(argv[1]));
  const auto image=View::RenderTerrain(field,camera,w,h);
  std::ofstream out(argv[2],std::ios::binary|std::ios::trunc);
  if(!out)throw std::runtime_error("cannot open output raster");
  View::WritePPM(image,out);
  std::cout<<"width="<<w<<" height="<<h<<" checksum="
           <<image.Checksum()<<'\n';
  return 0;
 }catch(const std::invalid_argument& e){
  std::cerr<<"spore_terrain_view: "<<e.what()<<'\n';return 2;
 }catch(const std::exception& e){
  std::cerr<<"spore_terrain_view: "<<e.what()<<'\n';return 1;
 }
}
