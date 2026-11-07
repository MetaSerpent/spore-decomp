#include "runtime/TerrainMesh.h"
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
float Real(const char* arg){
  char* end=nullptr;
  errno=0;
  const auto v=std::strtof(arg,&end);
  if(end==arg||*end!='\0'||errno!=0||!std::isfinite(v))
    throw std::invalid_argument("invalid real number");
  return v;
}
std::string Read(const std::string& path) {
  std::error_code ec;
  const auto size=std::filesystem::file_size(path,ec);
  if(ec||size>20000000)throw std::runtime_error("snapshot is missing or over 20 MB");
  std::ifstream in(path,std::ios::binary);
  if(!in)throw std::runtime_error("cannot open terrain snapshot");
  return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
}
int main(int argc,char** argv){
  try{
    if(argc<3||argc>5){
      std::cerr<<"Usage: spore_terrain_mesh <terrain.snapshot> <output.obj>"
                 " [horizontalScale=1] [verticalScale=1]\n";
      return 2;
    }
    const auto field=Spore::Terrain::TerraformField::Restore(Read(argv[1]));
    const auto horizontal=argc>=4?Real(argv[3]):1.f;
    const auto vertical=argc>=5?Real(argv[4]):1.f;
    if(!std::isfinite(horizontal)||horizontal<=0||
       !std::isfinite(vertical)||vertical<0||
       std::size_t(field.Width())*field.Height()>262144)
      throw std::invalid_argument("mesh resolution or scale invalid");
    std::ofstream out(argv[2],std::ios::binary|std::ios::trunc);
    if(!out)throw std::runtime_error("cannot create OBJ file");
    const auto stats=Spore::Terrain::WriteTerrainOBJ(field,out,horizontal,vertical);
    std::cout<<"vertices="<<stats.vertices<<" triangles="<<stats.triangles
             <<" min="<<stats.minimumHeight<<" max="<<stats.maximumHeight<<'\n';
    return 0;
  }catch(const std::invalid_argument& e){
    std::cerr<<"spore_terrain_mesh: "<<e.what()<<'\n';return 2;
  }catch(const std::exception& e){
    std::cerr<<"spore_terrain_mesh: "<<e.what()<<'\n';return 1;
  }
}
