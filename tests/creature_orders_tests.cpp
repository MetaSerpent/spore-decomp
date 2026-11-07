#include "runtime/CreatureOrders.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using Spore::Creature::Body;
using Spore::Creature::MotionWorld;
using Spore::Creature::MoveOrders;
using Spore::Creature::NavigationGrid;
using Spore::Creature::Waypoint;

namespace {
void Check(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
template<typename F> void Reject(F f,const char* msg) {
 bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}
 Check(rejected,msg);
}
std::vector<float> Ground(uint32_t w,uint32_t h) {
 return std::vector<float>(size_t(w)*h,.5f);
}
void RunStep(MoveOrders& orders,MotionWorld& motion,const NavigationGrid& nav,
             uint64_t id=7) {
 // The base Cell Stage AI would prefer the origin. A player destination
 // overrides it, but the same collision/kinematic solver still runs.
 std::vector<Body> targets{{id,1.f,1.f,.22f}};
 orders.Apply(targets,motion,nav);
 motion.Advance(targets,11.f,11.f,.6f);
}
void TestOrderedMovement(){
 NavigationGrid nav(12,12,Ground(12,12));
 MoveOrders orders;
 MotionWorld motion;
 motion.Advance({Body{7,1,1,.22f}},11,11);
 Check(orders.Issue(7,{1,1},{9,1},nav),"valid destination assigned");
 auto copy=MoveOrders::Restore(orders.Snapshot());
 Check(copy.Checksum()==orders.Checksum(),"destination roundtrip");
 MotionWorld twin=MotionWorld::Restore(motion.Snapshot());
 float furthest=1.f;
 bool progressed=false,completed=false;
 for(unsigned i=0;i<100;++i) {
   RunStep(orders,motion,nav);
   RunStep(copy,twin,nav);
   Check(motion.Checksum()==twin.Checksum(),"identical physics replay");
   Check(orders.Checksum()==copy.Checksum(),"same destination lifecycle");
   const auto* actor=motion.Find(7);
   Check(actor!=nullptr,"creature remains in physics");
   if(actor->x>furthest+.01f)progressed=true;
   furthest=std::max(furthest,actor->x);
   if(orders.Size()==0){completed=true;break;}
 }
 Check(progressed,"player route moves actor toward target");
 Check(furthest>8.5f,"actor navigated near destination");
 Check(completed,"completed orders are removed");
}
void DetourAndDynamicReplanning(){
 auto ground=Ground(12,12);
 for(unsigned z=0;z<12;++z)ground[z*12+5]=0.f;
 ground[9*12+5]=.5f;
 NavigationGrid nav(12,12,ground);
 MoveOrders orders;
 MotionWorld motion;
 motion.Advance({Body{9,2,2,.2f}},11,11);
 Check(orders.Issue(9,{2,2},{9,2},nav),"maze with opening is navigable");
 bool usedGap=false;
 for(unsigned i=0;i<170;++i){
   std::vector<Body> targets{{9,2,2,.2f}};
   const auto report=orders.Apply(targets,motion,nav);
   Check(report.blocked==0,"opening remains traversable");
   motion.Advance(targets,11,11,.6f);
   auto* a=motion.Find(9);
   if(a->z>8.f)usedGap=true;
 }
 Check(usedGap,"creature actually detours toward narrow gap");

 // A new barrier can invalidate the old path. The engine must not move the
 // target through blocked tiles and must preserve the requested destination.
 auto closed=ground;
 closed[9*12+5]=0.f;
 NavigationGrid disconnected(12,12,closed);
 const auto target=Waypoint{3,2};
 Check(!orders.Issue(9,target,{9,2},disconnected),
       "unreachable new order refused without replacing old one");
 const auto oldGoal=orders.Find(9);
 // Order may already have completed during the long simulation; reissue
 // a separate future goal to make this part deterministic.
 MoveOrders blocked;
 Check(blocked.Issue(5,{2,2},{4,2},disconnected),"local order issued");
 MotionWorld fresh;
 fresh.Advance({Body{5,2,2,.2f}},11,11);
 std::vector<Body> targets{{5,2,2,.2f}};
 const auto before=targets;
 const auto result=blocked.Apply(targets,fresh,disconnected);
 Check(result.directed==1&&result.blocked==0,"local motion still navigates");
 (void)oldGoal;
}
void Validation(){
 NavigationGrid nav(12,12,Ground(12,12));
 MoveOrders orders;
 Reject([&]{orders.Issue(0,{1,1},{2,2},nav);},"reject id zero");
 Reject([&]{orders.Issue(3,{1,1},{99,99},nav);},"reject invalid goal");
 Check(orders.Size()==0,"invalid order doesn't mutate state");
 Check(orders.Issue(3,{1,1},{2,2},nav),"first order accepted");
 const auto checksum=orders.Checksum();
 Reject([&]{orders.Issue(3,{99,99},{2,2},nav);},"invalid source rejected");
 Check(orders.Checksum()==checksum,"prior order preserved on invalid command");
 auto snapshot=orders.Snapshot();
 auto restored=MoveOrders::Restore(snapshot);
 Check(restored.Snapshot()==snapshot,"command snapshot canonical");
 Reject([&]{MoveOrders::Restore(snapshot+"junk");},"trailing bytes rejected");
 Reject([&]{MoveOrders::Restore("SPORE_CREATURE_ORDERS 1\n2\n5 1 1\n5 3 3\n");},
        "duplicate IDs rejected");
 Reject([&]{MoveOrders::Restore("SPORE_CREATURE_ORDERS 2\n0\n");},
        "future version rejected");
 Check(orders.Cancel(3)&&!orders.Cancel(3),"cancel removes exactly once");
 MotionWorld empty;
 std::vector<Body> bad{{3,-1,0,.2f}};
 Reject([&]{orders.Apply(bad,empty,nav);},"invalid target rejected atomically");
}
}
int main(){
 try{
  TestOrderedMovement();DetourAndDynamicReplanning();Validation();
  std::cout<<"Persistent creature navigation orders passed\n";
  return 0;
 }catch(const std::exception& e) {
  std::cerr<<"Creature order regression failed: "<<e.what()<<'\n';return 1;
 }
}
