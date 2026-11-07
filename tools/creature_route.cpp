#include "runtime/CreatureNavigation.h"

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {
uint32_t Unsigned(const char* value) {
  std::string_view input(value);
  if(input.empty()||input.front()=='-')throw std::invalid_argument("invalid unsigned coordinate");
  uint32_t result=0;
  const auto parsed=std::from_chars(input.data(),input.data()+input.size(),result);
  if(parsed.ec!=std::errc{}||parsed.ptr!=input.data()+input.size())
    throw std::invalid_argument("invalid unsigned coordinate");
  return result;
}
float Real(const char* arg) {
  char* end=nullptr;errno=0;
  const auto val=std::strtof(arg,&end);
  if(end==arg||*end!='\0'||errno||!std::isfinite(val))
    throw std::invalid_argument("invalid pathfinding option");
  return val;
}
std::string Load(const std::string& path) {
  std::error_code e;
  const auto bytes=std::filesystem::file_size(path,e);
  if(e||bytes>20*1024*1024)throw std::runtime_error("terrain snapshot missing or oversized");
  std::ifstream file(path,std::ios::binary);
  if(!file)throw std::runtime_error("cannot open snapshot");
  return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
void Usage() {
  std::cerr<<"Usage: spore_creature_route <terrain.snapshot> sx sz gx gz"
            " [minHeight=0.12] [maxStep=0.2] [slopePenalty=4]\n";
}
}
int main(int argc,char** argv) {
  try {
    if(argc<6||argc>9){Usage();return 2;}
    Spore::Creature::PathOptions options;
    if(argc>6)options.minimumHeight=Real(argv[6]);
    if(argc>7)options.maximumStep=Real(argv[7]);
    if(argc>8)options.slopePenalty=Real(argv[8]);
    auto field=Spore::Terrain::TerraformField::Restore(Load(argv[1]));
    const auto nav=Spore::Creature::NavigationGrid::FromTerrain(field,options);
    const auto path=nav.Route({Unsigned(argv[2]),Unsigned(argv[3])},
                              {Unsigned(argv[4]),Unsigned(argv[5])});
    if(path.nodes.empty()){
      std::cout<<"NO_ROUTE\n";
      return 3;
    }
    std::cout<<"x,z\n";
    for(const auto p:path.nodes)std::cout<<p.x<<','<<p.z<<'\n';
    std::cerr<<"cost_milli="<<path.costMilli<<" expanded="<<path.expanded<<'\n';
    return 0;
  }catch(const std::invalid_argument& e) {
    std::cerr<<"spore_creature_route: "<<e.what()<<'\n';return 2;
  }catch(const std::out_of_range& e) {
    std::cerr<<"spore_creature_route: "<<e.what()<<'\n';return 2;
  }catch(const std::exception& e) {
    std::cerr<<"spore_creature_route: "<<e.what()<<'\n';return 1;
  }
}
