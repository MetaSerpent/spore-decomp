#include "runtime/CreatureNavigation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace Spore::Creature {
namespace {
constexpr size_t kMaxNodes=262144;
constexpr uint64_t kUnvisited=UINT64_MAX;
struct OpenNode {
  uint64_t f=0,g=0;
  uint32_t index=0;
};
// A* priority is deterministic across all compliant STL implementations:
// smaller estimated total cost, then smaller path cost, then smaller index.
struct Worse {
  bool operator()(const OpenNode& a,const OpenNode& b) const {
    return std::tie(a.f,a.g,a.index)>std::tie(b.f,b.g,b.index);
  }
};
uint64_t Heuristic(uint32_t w,uint32_t a,uint32_t b) {
  const auto ax=a%w,az=a/w,bx=b%w,bz=b/w;
  const uint64_t dx=ax>bx?ax-bx:bx-ax;
  const uint64_t dz=az>bz?az-bz:bz-az;
  return (dx+dz)*UINT64_C(1000);
}
bool ValidOptions(const PathOptions& p) {
  return std::isfinite(p.minimumHeight)&&p.minimumHeight>=0&&p.minimumHeight<=1&&
    std::isfinite(p.maximumStep)&&p.maximumStep>=0&&p.maximumStep<=1&&
    std::isfinite(p.slopePenalty)&&p.slopePenalty>=0&&p.slopePenalty<=100;
}
}
NavigationGrid::NavigationGrid(uint32_t w,uint32_t h,
                               std::vector<float> heights,PathOptions options)
  :width_(w),height_(h),heights_(std::move(heights)),options_(options) {
  if(w<2||h<2||uint64_t(w)*h>kMaxNodes||
     heights_.size()!=uint64_t(w)*h||!ValidOptions(options_))
    throw std::invalid_argument("invalid creature navigation dimensions or parameters");
  for(const float elevation:heights_)
    if(!std::isfinite(elevation)||elevation<0||elevation>1)
      throw std::invalid_argument("navigation height is not finite within [0,1]");
}
NavigationGrid NavigationGrid::FromTerrain(const Terrain::TerraformField& field,
                                           PathOptions options) {
  return NavigationGrid(field.Width(),field.Height(),field.Samples(),options);
}
bool NavigationGrid::Walkable(uint32_t x,uint32_t z) const noexcept {
  return x<width_&&z<height_&&heights_[Index(x,z)]>=options_.minimumHeight;
}
bool NavigationGrid::CanTraverse(Waypoint a,Waypoint b) const noexcept {
  if(!Walkable(a.x,a.z)||!Walkable(b.x,b.z))return false;
  const uint64_t dx=a.x>b.x?a.x-b.x:b.x-a.x;
  const uint64_t dz=a.z>b.z?a.z-b.z:b.z-a.z;
  if(dx+dz!=1)return false;
  return std::fabs(heights_[Index(a.x,a.z)]-heights_[Index(b.x,b.z)])
         <=options_.maximumStep;
}
PathResult NavigationGrid::Route(Waypoint start,Waypoint goal) const {
  if(start.x>=width_||start.z>=height_||goal.x>=width_||goal.z>=height_)
    throw std::out_of_range("creature navigation endpoint outside grid");
  PathResult result;
  if(!Walkable(start.x,start.z)||!Walkable(goal.x,goal.z))return result;
  const uint32_t source=Index(start.x,start.z);
  const uint32_t destination=Index(goal.x,goal.z);
  const auto count=heights_.size();
  std::vector<uint64_t> costs(count,kUnvisited);
  std::vector<uint32_t> parents(count,UINT32_MAX);
  std::priority_queue<OpenNode,std::vector<OpenNode>,Worse> open;
  costs[source]=0;
  open.push({Heuristic(width_,source,destination),0,source});
  const auto push=[&](uint32_t from,uint32_t to) {
    const Waypoint a{from%width_,from/width_};
    const Waypoint b{to%width_,to/width_};
    if(!CanTraverse(a,b))return;
    const double rise=std::fabs(double(heights_[to])-double(heights_[from]));
    const uint64_t extra=static_cast<uint64_t>(
        std::llround(rise*double(options_.slopePenalty)*1000.0));
    const uint64_t next=costs[from]+1000+extra;
    if(next>=costs[to])return;
    costs[to]=next;
    parents[to]=from;
    open.push({next+Heuristic(width_,to,destination),next,to});
  };
  while(!open.empty()) {
    const auto current=open.top();open.pop();
    if(costs[current.index]!=current.g)continue; // stale queue entry
    ++result.expanded;
    if(current.index==destination)break;
    const uint32_t x=current.index%width_,z=current.index/width_;
    // Fixed cardinal order; A* queue index tie-breaking is portable.
    if(z>0)push(current.index,current.index-width_);
    if(x>0)push(current.index,current.index-1);
    if(x+1<width_)push(current.index,current.index+1);
    if(z+1<height_)push(current.index,current.index+width_);
  }
  if(costs[destination]==kUnvisited)return result;
  result.costMilli=costs[destination];
  uint32_t current=destination;
  while(true) {
    result.nodes.push_back({current%width_,current/width_});
    if(current==source)break;
    const uint32_t previous=parents[current];
    if(previous==UINT32_MAX||result.nodes.size()>count)
      throw std::logic_error("inconsistent navigation predecessor chain");
    current=previous;
  }
  std::reverse(result.nodes.begin(),result.nodes.end());
  return result;
}
} // namespace Spore::Creature
