#include "runtime/CreatureOrders.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace Spore::Creature {
namespace {
constexpr size_t kMaxOrders=4096;
uint32_t Tile(float value,uint32_t max) {
  return static_cast<uint32_t>(std::clamp(
    std::lround(value),0L,static_cast<long>(max)));
}
bool InBounds(Waypoint point,const NavigationGrid& grid) {
  return point.x<grid.Width()&&point.z<grid.Height();
}
constexpr uint64_t kFNVOffset=UINT64_C(14695981039346656037);
constexpr uint64_t kFNVPrime=UINT64_C(1099511628211);
}
bool MoveOrders::Issue(uint64_t actor,Waypoint from,Waypoint destination,
                       const NavigationGrid& terrain) {
  if(actor==0||!InBounds(from,terrain)||!InBounds(destination,terrain))
    throw std::invalid_argument("invalid creature destination order");
  if(destinations_.size()>=kMaxOrders && !destinations_.count(actor))
    throw std::invalid_argument("too many active creature orders");
  const auto path=terrain.Route(from,destination);
  if(path.nodes.empty())return false;
  destinations_[actor]={destination.x,destination.z};
  return true;
}
bool MoveOrders::Cancel(uint64_t actor) noexcept {
  return destinations_.erase(actor)!=0;
}
const Destination* MoveOrders::Find(uint64_t actor) const noexcept {
  const auto found=destinations_.find(actor);
  return found==destinations_.end()?nullptr:&found->second;
}
OrderStep MoveOrders::Apply(std::vector<Body>& targets,
                            const MotionWorld& current,
                            const NavigationGrid& terrain) {
  // Validate before modifying even a single outgoing desired body.
  for(const auto& target:targets)
    if(target.id==0||!std::isfinite(target.x)||!std::isfinite(target.z)||
       target.x<0||target.z<0||target.x>terrain.Width()-1||
       target.z>terrain.Height()-1)
      throw std::invalid_argument("invalid creature target for navigation");
  auto candidate=targets;
  auto saved=destinations_;
  OrderStep report;
  for(auto it=saved.begin();it!=saved.end();) {
    auto body=std::find_if(candidate.begin(),candidate.end(),
        [&](const Body& b){return b.id==it->first;});
    if(body==candidate.end()) {
      ++report.removed;
      it=saved.erase(it);
      continue;
    }
    const auto* actual=current.Find(body->id);
    const float x=actual?actual->x:body->x;
    const float z=actual?actual->z:body->z;
    const auto goal=Waypoint{it->second.x,it->second.z};
    if(!InBounds(goal,terrain)) {
      ++report.blocked;
      ++it;
      continue;
    }
    const float dx=x-float(goal.x),dz=z-float(goal.z);
    if(dx*dx+dz*dz<=.16f) {
      ++report.completed;
      it=saved.erase(it);
      continue;
    }
    const auto start=Waypoint{Tile(x,terrain.Width()-1),
                              Tile(z,terrain.Height()-1)};
    const auto plan=terrain.Route(start,goal);
    if(plan.nodes.empty()) {
      ++report.blocked;
      ++it;
      continue;
    }
    const auto next=plan.nodes.size()>1?plan.nodes[1]:plan.nodes[0];
    body->x=float(next.x);
    body->z=float(next.z);
    ++report.directed;
    ++it;
  }
  targets=std::move(candidate);
  destinations_=std::move(saved);
  return report;
}
std::string MoveOrders::Snapshot() const {
  std::ostringstream out;
  out<<"SPORE_CREATURE_ORDERS 1\n"<<destinations_.size()<<'\n';
  for(const auto& [id,where]:destinations_)
    out<<id<<' '<<where.x<<' '<<where.z<<'\n';
  return out.str();
}
MoveOrders MoveOrders::Restore(std::string_view data) {
  if(data.size()>256000)
    throw std::invalid_argument("creature order snapshot exceeds limit");
  std::istringstream in{std::string(data)};
  std::string magic;
  unsigned version=0;
  size_t count=0;
  if(!(in>>magic>>version)||magic!="SPORE_CREATURE_ORDERS"||
     version!=1||!(in>>count)||count>kMaxOrders)
    throw std::invalid_argument("invalid creature order snapshot header");
  MoveOrders result;
  uint64_t lastId=0;
  for(size_t i=0;i<count;++i) {
    uint64_t id=0;
    uint32_t x=0,z=0;
    if(!(in>>id>>x>>z)||id<=lastId||x>262144||z>262144)
      throw std::invalid_argument("invalid creature destination record");
    lastId=id;
    result.destinations_.emplace(id,Destination{x,z});
  }
  std::string trailing;
  if(in>>trailing)
    throw std::invalid_argument("trailing creature order snapshot data");
  return result;
}
uint64_t MoveOrders::Checksum() const noexcept {
  uint64_t hash=kFNVOffset;
  // Hash stable integers directly; no locale or serialization dependence.
  auto append=[&hash](uint64_t value){
    for(unsigned i=0;i<8;++i){
      hash^=static_cast<uint8_t>(value>>(i*8));
      hash*=kFNVPrime;
    }
  };
  append(destinations_.size());
  for(const auto& [id,where]:destinations_){
    append(id);append(where.x);append(where.z);
  }
  return hash;
}
} // namespace Spore::Creature
