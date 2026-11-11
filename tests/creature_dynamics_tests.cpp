#include "runtime/CreatureDynamics.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Spore::Creature;
namespace {
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
template<typename F> void rejected(F f,const char* why) {
 bool yes=false;try{f();}catch(const std::invalid_argument&){yes=true;}
 check(yes,why);
}
std::vector<Body> scenario(unsigned n) {
 std::vector<Body> targets;
 for(unsigned i=0;i<n;++i){
  targets.push_back({uint64_t(i+1),
        .5f+float(i%23)*.4f,
        .5f+float(i/23)*.6f,.18f+.002f*float(i%10)});
 }
 return targets;
}
void deterministic() {
 auto targets=scenario(100);
 MotionWorld one,two;
 for(unsigned i=0;i<50;++i) {
  for(size_t j=0;j<targets.size();++j) {
    targets[j].x=.6f+float(j%23)*.4f+.1f*std::sin(float(i)*.2f);
    targets[j].z=.6f+float(j/23)*.6f+.08f*std::cos(float(i)*.2f);
  }
  const auto stat1=one.Advance(targets,12,12);
  std::reverse(targets.begin(),targets.end());
  const auto stat2=two.Advance(targets,12,12);
  std::reverse(targets.begin(),targets.end());
  check(stat1.pairs==stat2.pairs && stat1.corrections==stat2.corrections,
        "broadphase contacts independent of input order");
  check(one.Checksum()==two.Checksum(),"state replay independent of actor order");
  check(one.Snapshot()==two.Snapshot(),"snapshots independent of input order");
  check(one.Bodies().size()==100,"all current actors retained");
 }
 check(one.Steps()==50,"50 deterministic motion frames");
 auto encoded=one.Snapshot();
 auto restored=MotionWorld::Restore(encoded);
 check(restored.Checksum()==one.Checksum(),"restore preserves checksum");
 check(restored.Snapshot()==encoded,"motion snapshot roundtrip exact");
 for(unsigned i=0;i<100;++i){
  auto positions=scenario(100);
  one.Advance(positions,12,12,.42f);
  restored.Advance(positions,12,12,.42f);
  check(restored.Checksum()==one.Checksum(),"post-restore physics replay");
 }
}
void spawns_contacts_and_validation() {
 MotionWorld state;
 std::vector<Body> targets{{10,2,2,.40f},{20,2,2,.40f}};
 auto initial=state.Advance(targets,8,8);
 check(initial.pairs==1,"spawned overlapping bodies collide");
 const auto* a=state.Find(10);
 const auto* b=state.Find(20);
 check(a && b && std::hypot(a->x-b->x,a->z-b->z)>.75f,
       "contact physically separates spawned actors");
 check(!state.Find(100),"unknown organism not in motion world");
 const auto initialHash=state.Checksum();
 rejected([&]{state.Advance({{10,2,2,.4f},{10,2,2,.4f}},8,8);},
          "duplicate ID rejected");
 check(state.Checksum()==initialHash,"validation failure is transactional");
 rejected([&]{state.Advance(targets,8,8,-.1f);},"invalid response rejected");
 rejected([&]{state.Advance({{12,-1,1,.2f}},8,8);},"invalid target rejected");
 check(state.Checksum()==initialHash,"bad targets preserve the old state");
 targets={{20,2,2,.4f}};
 state.Advance(targets,8,8);
 check(state.Bodies().size()==1&&!state.Find(10),"dead actors pruned");
 state.Advance({},8,8);
 check(state.Bodies().empty(),"empty target list clears all motion actors");
}
void corruption(){
 MotionWorld original;
 original.Advance({{1,3,3,.3f}},8,8);
 const auto bytes=original.Snapshot();
 rejected([&]{MotionWorld::Restore("");},"empty snapshot");
 rejected([&]{MotionWorld::Restore(bytes.substr(0,bytes.size()/2));},
          "truncated snapshot");
 rejected([&]{MotionWorld::Restore(bytes+"garbage");},
          "trailing bytes");
 rejected([&]{MotionWorld::Restore("SPORE_CREATURE_MOTION 3\n0 0\n");},
          "unsupported version");
 rejected([&]{MotionWorld::Restore("SPORE_CREATURE_MOTION 1\n0 2\n"
         "4 1 1 0 0 .3\n3 1 1 0 0 .3\n");},
          "unordered body IDs");
 rejected([&]{MotionWorld::Restore("SPORE_CREATURE_MOTION 1\n0 1\n"
         "4 nan 1 0 0 .3\n");},
          "nonfinite saved positions");
}
void authoritative_positions(){
 MotionWorld state;
 state.Synchronize({{2,3.25f,2.5f,1.5f,-.25f,.3f},{1,.625f,1,0,0,.25f}},8,8);
 check(state.Find(1)->x==.625f&&state.Find(2)->velocityX==1.5f,"simulation poses must not acquire spring lag");
 const auto bytes=state.Snapshot();
 check(MotionWorld::Restore(bytes).Snapshot()==bytes,"authoritative poses and velocity replay exactly");
 rejected([&]{state.Synchronize({{1,1,1,0,0,.3f},{1,2,2,0,0,.3f}},8,8);},"duplicate authoritative IDs");
 check(state.Snapshot()==bytes,"bad authoritative input leaves state unchanged");
 rejected([&]{state.Synchronize({{1,9,1,0,0,.3f}},8,8);},"authoritative pose outside bounds");
 check(state.Snapshot()==bytes,"bounds failure leaves state unchanged");
}
}
int main(){
 try{
  deterministic();spawns_contacts_and_validation();corruption();authoritative_positions();
  std::cout<<"Persistent creature motion/collision tests passed\n";
  return 0;
 }catch(const std::exception&e){
  std::cerr<<"Creature dynamics regression failed: "<<e.what()<<'\n';
  return 1;
 }
}
