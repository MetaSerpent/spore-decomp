#include "runtime/CreatureNavigation.h"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using Spore::Creature::NavigationGrid;
using Spore::Creature::Waypoint;
using Spore::Creature::PathOptions;

namespace {
void Check(bool test,const char* why){if(!test)throw std::runtime_error(why);}
template<typename F> void Reject(F fn,const char* reason){
  bool rejected=false;
  try{fn();}catch(const std::exception&){rejected=true;}
  Check(rejected,reason);
}
std::vector<float> Flat(uint32_t w,uint32_t h,float value=.5f){
  return std::vector<float>(size_t(w)*h,value);
}
void DetourAndUnreachable(){
  constexpr uint32_t w=9,h=9;
  auto terrain=Flat(w,h);
  for(uint32_t z=0;z<h;++z)terrain[size_t(z)*w+4]=0; // impassable wall
  terrain[size_t(7)*w+4]=.5f; // one traversable opening
  NavigationGrid nav(w,h,terrain);
  const auto plan=nav.Route({2,1},{6,1});
  Check(!plan.nodes.empty(),"route around wall found");
  Check(plan.nodes.front().x==2&&plan.nodes.front().z==1,"route begins at start");
  Check(plan.nodes.back().x==6&&plan.nodes.back().z==1,"route ends at target");
  Check(plan.costMilli==16000,"optimal 16-cardinal-step detour cost");
  bool crossedOpening=false;
  for(size_t i=0;i<plan.nodes.size();++i){
    const auto p=plan.nodes[i];
    if(p.x==4&&p.z==7)crossedOpening=true;
    if(i)Check(nav.CanTraverse(plan.nodes[i-1],p),"all route steps traversable");
  }
  Check(crossedOpening,"path chooses opening through obstacle wall");
  Check(plan.expanded>0&&plan.expanded<=size_t(w)*h,"bounded A* expansion");
  const auto again=nav.Route({2,1},{6,1});
  Check(plan.nodes.size()==again.nodes.size()&&plan.costMilli==again.costMilli,
        "deterministic A* replay path length");
  for(size_t i=0;i<plan.nodes.size();++i)
    Check(plan.nodes[i].x==again.nodes[i].x&&
          plan.nodes[i].z==again.nodes[i].z,"same path tie-breaking");
  terrain[size_t(7)*w+4]=0;
  const auto isolated=NavigationGrid(w,h,terrain).Route({2,1},{6,1});
  Check(isolated.nodes.empty()&&isolated.costMilli==0,"unreachable goal is explicit");
}
void SlopeCostAndTerrain(){
  auto heights=Flat(5,4);
  heights[1*5+2]=.9f;
  PathOptions options;options.maximumStep=1.f;options.slopePenalty=100.f;
  NavigationGrid steep(5,4,heights,options);
  const auto plan=steep.Route({0,1},{4,1});
  Check(plan.costMilli==6000,"high slope cost prefers flatter six-step bypass");
  for(const auto p:plan.nodes)
    Check(!(p.x==2&&p.z==1),"avoids steep plateau");
  options.maximumStep=.1f;options.slopePenalty=0;
  NavigationGrid hardStep(5,4,heights,options);
  const auto safe=hardStep.Route({0,1},{4,1});
  Check(safe.costMilli==6000,"step height limit blocks steep transition");
  Spore::Terrain::TerraformField terrain(16,16,.5f);
  terrain.Apply(.5f,.5f,.3f,.4f);
  const auto fromField=NavigationGrid::FromTerrain(terrain);
  Check(fromField.Width()==16&&fromField.Height()==16,"terrain adapter maps grid");
  const auto path=fromField.Route({0,0},{15,15});
  Check(!path.nodes.empty()&&path.costMilli>=30000,"terrain route exists");
}
void ValidationAndInvariants(){
  Reject([]{NavigationGrid nav(1,5,Flat(1,5));},"minimum map width");
  Reject([]{NavigationGrid nav(8,8,Flat(3,8));},"map samples must match");
  Reject([]{NavigationGrid nav(1000,1000,Flat(2,2));},"map upper bound");
  auto heights=Flat(4,4);
  heights[0]=std::numeric_limits<float>::quiet_NaN();
  Reject([&]{NavigationGrid nav(4,4,heights);},"NaN height rejected");
  heights=Flat(4,4);
  PathOptions invalid;invalid.maximumStep=-.1f;
  Reject([&]{NavigationGrid nav(4,4,heights,invalid);},"negative slope blocked");
  NavigationGrid nav(4,4,heights);
  Reject([&]{nav.Route({4,0},{0,0});},"invalid source bounds");
  Reject([&]{nav.Route({0,0},{4,0});},"invalid destination bounds");
  Check(nav.Route({2,2},{2,2}).nodes.size()==1,"zero-length path one node");
  Check(nav.Route({2,2},{2,2}).costMilli==0,"zero-length path zero cost");
  Check(!nav.CanTraverse({0,0},{1,1}),"diagonal traversal forbidden");
  Check(!nav.CanTraverse({0,0},{0,0}),"no zero-length step traversal");
  // Identical input repeated with changing waypoint queries never mutates grid.
  for(unsigned i=0;i<500;++i){
    const auto p=nav.Route({0,0},{3,3});
    Check(p.costMilli==6000&&p.nodes.size()==7,"repeated routes stable");
  }
}
}
int main(){
 try{DetourAndUnreachable();SlopeCostAndTerrain();ValidationAndInvariants();
     std::cout<<"Creature A* navigation tests passed\n";return 0;}
 catch(const std::exception& e){std::cerr<<"Navigation test failed: "<<e.what()<<'\n';return 1;}
}
