#include "runtime/CreaturePhysics.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Spore::Creature {
namespace {
using Bucket=std::pair<int,int>;
using Buckets=std::map<Bucket,std::vector<size_t>>;
bool valid(const Body& b,float w,float h) {
  return b.id!=0&&std::isfinite(b.x)&&std::isfinite(b.z)&&
         std::isfinite(b.radius)&&b.radius>=.05f&&b.radius<=.49f&&
         b.x>=0.f&&b.z>=0.f&&b.x<=w&&b.z<=h;
}
void clamp(Body& b,float w,float h){
  b.x=std::clamp(b.x,b.radius,w-b.radius);
  b.z=std::clamp(b.z,b.radius,h-b.radius);
}
Buckets MakeBuckets(const std::vector<Body>& bodies,
                    const std::vector<size_t>& order) {
  Buckets buckets;
  for(size_t i:order) {
    const auto& body=bodies[i];
    buckets[{int(std::floor(body.x)),int(std::floor(body.z))}].push_back(i);
  }
  return buckets;
}
float depth(const Body& a,const Body& b) {
  const double dx=double(b.x)-double(a.x);
  const double dz=double(b.z)-double(a.z);
  return float(double(a.radius)+double(b.radius)-std::sqrt(dx*dx+dz*dz));
}
template<typename F>
void ForPairs(const std::vector<Body>& bodies,
              const std::vector<size_t>& order,const Buckets& buckets,
              F&& f){
  for(size_t i:order) {
    const auto& a=bodies[i];
    const int x=int(std::floor(a.x));
    const int z=int(std::floor(a.z));
    for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx) {
      const auto found=buckets.find({x+dx,z+dz});
      if(found==buckets.end())continue;
      for(size_t j:found->second){
        if(bodies[j].id<=a.id)continue;
        f(i,j);
      }
    }
  }
}
} // namespace

ContactSummary ResolveContacts(std::vector<Body>& bodies,
                               float w,float h,unsigned passes){
  if(!std::isfinite(w)||!std::isfinite(h)||w<2.f||h<2.f||
     w>100000.f||h>100000.f||passes==0||passes>12||
     bodies.size()>16384)
    throw std::invalid_argument("invalid collision world or iteration count");
  std::vector<size_t> order(bodies.size());
  std::iota(order.begin(),order.end(),0);
  std::sort(order.begin(),order.end(),[&](size_t a,size_t b){
    return bodies[a].id<bodies[b].id;
  });
  for(size_t k=0;k<order.size();++k){
    if(!valid(bodies[order[k]],w,h)||
       (k&&bodies[order[k]].id==bodies[order[k-1]].id))
      throw std::invalid_argument("invalid or duplicate collision body");
  }
  for(auto& body:bodies)clamp(body,w,h);
  ContactSummary report;
  for(unsigned pass=0;pass<passes;++pass){
    const auto buckets=MakeBuckets(bodies,order);
    ForPairs(bodies,order,buckets,[&](size_t i,size_t j){
      auto& a=bodies[i];auto& b=bodies[j];
      const auto overlap=depth(a,b);
      if(overlap<=.000001f)return;
      if(pass==0)++report.pairs;
      ++report.corrections;
      const double dx=double(b.x)-double(a.x);
      const double dz=double(b.z)-double(a.z);
      const double len=std::sqrt(dx*dx+dz*dz);
      // When centres coincide, alternate deterministic X alignment by ID.
      const float nx=len<1e-10?(a.id<b.id?1.f:-1.f):float(dx/len);
      const float nz=len<1e-10?0.f:float(dz/len);
      const float step=(overlap+.00001f)*.5f;
      a.x-=nx*step;a.z-=nz*step;
      b.x+=nx*step;b.z+=nz*step;
      clamp(a,w,h);clamp(b,w,h);
    });
  }
  // For many bodies in a tiny area, overlaps cannot all be resolved.
  // Report the honest residual rather than claiming perfect separation.
  const auto buckets=MakeBuckets(bodies,order);
  ForPairs(bodies,order,buckets,[&](size_t i,size_t j){
    report.maxRemainingDepth=std::max(report.maxRemainingDepth,
                                      depth(bodies[i],bodies[j]));
  });
  return report;
}

} // namespace Spore::Creature
