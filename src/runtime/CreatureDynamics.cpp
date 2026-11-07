#include "runtime/CreatureDynamics.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace Spore::Creature {
namespace {
constexpr size_t maxActors=16384;
constexpr uint64_t offset=UINT64_C(14695981039346656037);
constexpr uint64_t prime=UINT64_C(1099511628211);
bool Valid(const MotionBody& b) {
  return b.id!=0&&std::isfinite(b.x)&&std::isfinite(b.z)&&
         std::isfinite(b.velocityX)&&std::isfinite(b.velocityZ)&&
         std::isfinite(b.radius)&&b.radius>=.05f&&b.radius<=.49f&&
         b.x>=0.f&&b.z>=0.f&&b.x<=100000.f&&b.z<=100000.f&&
         std::fabs(b.velocityX)<=64.f&&std::fabs(b.velocityZ)<=64.f;
}
void HashWord(uint64_t& hash,uint32_t v) noexcept {
  for(unsigned i=0;i<4;++i){hash^=uint8_t(v>>(i*8));hash*=prime;}
}
void HashFloat(uint64_t& hash,float v) noexcept {
  uint32_t bits=0;
  std::memcpy(&bits,&v,sizeof bits);
  HashWord(hash,bits);
}
} // namespace

ContactSummary MotionWorld::Advance(const std::vector<Body>& targets,
                                     float w,float h,float response) {
  if(!std::isfinite(w)||!std::isfinite(h)||w<2||h<2||w>100000||h>100000||
     !std::isfinite(response)||response<=0||response>1||
     targets.size()>maxActors)
    throw std::invalid_argument("invalid motion world dimensions or responsiveness");
  auto sorted=targets;
  std::sort(sorted.begin(),sorted.end(),[](const Body& a,const Body& b){
    return a.id<b.id;
  });
  for(size_t i=0;i<sorted.size();++i) {
    const auto& target=sorted[i];
    if(target.id==0||!std::isfinite(target.x)||!std::isfinite(target.z)||
       !std::isfinite(target.radius)||target.radius<.05f||
       target.radius>.49f||target.x<0||target.x>w||
       target.z<0||target.z>h||
       (i>0&&target.id==sorted[i-1].id))
      throw std::invalid_argument("invalid or repeated creature motion target");
  }
  std::vector<Body> positions;
  positions.reserve(sorted.size());
  std::vector<MotionBody> updated;
  updated.reserve(sorted.size());
  for(const auto& target:sorted) {
    MotionBody actor;
    actor.id=target.id;
    actor.radius=target.radius;
    if(const auto* old=Find(target.id)) {
      actor.velocityX=std::clamp(old->velocityX*.35f+
                                (target.x-old->x)*response,-2.f,2.f);
      actor.velocityZ=std::clamp(old->velocityZ*.35f+
                                (target.z-old->z)*response,-2.f,2.f);
      actor.x=std::clamp(old->x+actor.velocityX,actor.radius,w-actor.radius);
      actor.z=std::clamp(old->z+actor.velocityZ,actor.radius,h-actor.radius);
    } else {
      actor.x=std::clamp(target.x,actor.radius,w-actor.radius);
      actor.z=std::clamp(target.z,actor.radius,h-actor.radius);
    }
    positions.push_back({actor.id,actor.x,actor.z,actor.radius});
    updated.push_back(actor);
  }
  const auto result=ResolveContacts(positions,w,h,6);
  for(size_t i=0;i<updated.size();++i) {
    auto& actor=updated[i];
    const auto& corrected=positions[i];
    actor.velocityX=std::clamp(actor.velocityX+(corrected.x-actor.x),-2.f,2.f);
    actor.velocityZ=std::clamp(actor.velocityZ+(corrected.z-actor.z),-2.f,2.f);
    actor.x=corrected.x;
    actor.z=corrected.z;
  }
  if(steps_==UINT64_MAX)
    throw std::overflow_error("motion step counter overflow");
  bodies_=std::move(updated);
  ++steps_;
  return result;
}
const MotionBody* MotionWorld::Find(uint64_t id) const noexcept {
  const auto it=std::lower_bound(bodies_.begin(),bodies_.end(),id,
      [](const MotionBody& b,uint64_t target){return b.id<target;});
  return it==bodies_.end()||it->id!=id?nullptr:&*it;
}
uint64_t MotionWorld::Checksum() const noexcept {
  uint64_t h=offset;
  HashWord(h,uint32_t(steps_));HashWord(h,uint32_t(steps_>>32));
  HashWord(h,uint32_t(bodies_.size()));
  for(const auto& b:bodies_){
    HashWord(h,uint32_t(b.id));HashWord(h,uint32_t(b.id>>32));
    HashFloat(h,b.x);HashFloat(h,b.z);
    HashFloat(h,b.velocityX);HashFloat(h,b.velocityZ);
    HashFloat(h,b.radius);
  }
  return h;
}
std::string MotionWorld::Snapshot() const {
  std::ostringstream out;
  out<<"SPORE_CREATURE_MOTION 1\n"<<steps_<<' '<<bodies_.size()<<'\n';
  out<<std::setprecision(std::numeric_limits<float>::max_digits10);
  for(const auto& b:bodies_)
    out<<b.id<<' '<<b.x<<' '<<b.z<<' '<<b.velocityX<<' '
       <<b.velocityZ<<' '<<b.radius<<'\n';
  return out.str();
}
MotionWorld MotionWorld::Restore(std::string_view bytes) {
  if(bytes.size()>2*1024*1024)
    throw std::invalid_argument("oversized creature motion snapshot");
  std::istringstream in{std::string(bytes)};
  std::string magic;unsigned version=0;
  MotionWorld out;
  size_t count=0;
  if(!(in>>magic>>version)||magic!="SPORE_CREATURE_MOTION"||version!=1||
     !(in>>out.steps_>>count)||count>maxActors)
    throw std::invalid_argument("invalid creature motion snapshot header");
  out.bodies_.reserve(count);
  uint64_t last=0;
  for(size_t i=0;i<count;++i){
    MotionBody b;
    if(!(in>>b.id>>b.x>>b.z>>b.velocityX>>b.velocityZ>>b.radius)||
       !Valid(b)||b.id<=last)
      throw std::invalid_argument("invalid creature motion body");
    last=b.id;
    out.bodies_.push_back(b);
  }
  std::string garbage;
  if(in>>garbage)
    throw std::invalid_argument("unexpected trailing creature motion bytes");
  return out;
}
} // namespace Spore::Creature
