#include "runtime/CreatureGeometry.h"
#include "runtime/CreaturePhysics.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace Spore::Creature;
namespace {
void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
template<class F> void reject(F fn,const char* message){
 bool threw=false;
 try{fn();}catch(const std::invalid_argument&){threw=true;}
 require(threw,message);
}
void geometry() {
 const Traits t{.8f,.9f,.4f,.95f,0x00D08051};
 const auto first=GenerateCreature(t,0.f,8,4);
 const auto again=GenerateCreature(t,0.f,8,4);
 require(!first.vertices.empty() && !first.triangles.empty(),
         "mesh has vertices and faces");
 require(first.vertices.size()==again.vertices.size() &&
         first.triangles.size()==again.triangles.size(),"repeatable topology");
 for(size_t i=0;i<first.vertices.size();++i){
   const auto v=first.vertices[i],other=again.vertices[i];
   require(v.x==other.x&&v.y==other.y&&v.z==other.z,"deterministic vertex positions");
   require(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z),
           "finite vertex positions");
   require(v.y>=-.1f&&v.y<=2.f&&std::abs(v.x)<2.f&&std::abs(v.z)<2.f,
           "bounded creature anatomy");
 }
 for(const auto tri:first.triangles)
   require(tri.a<first.vertices.size()&&tri.b<first.vertices.size()&&
           tri.c<first.vertices.size(),"valid indexed triangles");
 const auto moved=GenerateCreature(t,1.5f,8,4);
 require(moved.vertices.size()==first.vertices.size(),"animation preserves mesh topology");
 size_t changed=0;
 for(size_t i=0;i<first.vertices.size();++i)
   if(first.vertices[i].y!=moved.vertices[i].y||
      first.vertices[i].z!=moved.vertices[i].z)++changed;
 require(changed>15,"gait animates legs and tail");
 require(GenerateCreature(t,6.283185307179586f,8,4).vertices.size()
         ==first.vertices.size(),"cycle preserves geometry");
 const Traits smaller{.1f,.1f,.1f,.1f,0x0099FF00};
 const auto little=GenerateCreature(smaller,0.f,8,4);
 require(little.triangles.size()<first.triangles.size(),
         "defensive spines create extra geometry");
 require(FootprintRadius(.9f)>FootprintRadius(.1f),
         "body size controls physical footprint");
 std::ostringstream obj;
 WriteCreatureOBJ(first,obj);
 require(obj.str().find("\no BlackSporeCreature\n")!=std::string::npos,
         "OBJ identifies procedural creature");
 require(obj.str().find("\nv ")!=std::string::npos&&
         obj.str().find("\nf ")!=std::string::npos,"OBJ includes mesh geometry");
 reject([]{GenerateCreature(Traits{},std::numeric_limits<float>::infinity());},
        "nonfinite phase rejected");
 reject([]{GenerateCreature(Traits{},0.f,1000);},"tessellation bound");
 reject([]{Traits invalid;invalid.bodySize=-1;GenerateCreature(invalid,0.f);},
        "bad genes rejected");
 reject([]{FootprintRadius(-1);},"radius rejects invalid genes");
}
void collisions(){
 std::vector<Body> b{{1,3.0f,3.0f,.35f},{2,3.2f,3.f,.35f},
                     {3,5.0f,4.0f,.35f}};
 const auto stat=ResolveContacts(b,10,10,8);
 require(stat.pairs==1&&stat.corrections>=1,"resolve overlapping pair");
 require(stat.maxRemainingDepth<.0002f,"separate two overlapping bodies");
 require(b[2].x==5.f&&b[2].z==4.f,"unrelated actor unaffected");
 const auto d=std::hypot(b[0].x-b[1].x,b[0].z-b[1].z);
 require(d>=b[0].radius+b[1].radius-.0002f,"no collision penetration");
 auto c=b;
 require(ResolveContacts(c,10,10).pairs==0,"collision resolution stable");
 auto shuffled=std::vector<Body>{{3,5.f,4.f,.35f},{2,3.2f,3.f,.35f},
                                  {1,3.f,3.f,.35f}};
 ResolveContacts(shuffled,10,10,8);
 std::sort(shuffled.begin(),shuffled.end(),
           [](const Body&a,const Body&b){return a.id<b.id;});
 require(std::abs(shuffled[0].x-b[0].x)<.00001f &&
         std::abs(shuffled[1].x-b[1].x)<.00001f,
         "contact order independent of input ordering");
 std::vector<Body> same{{1,2.f,2.f,.4f},{2,2.f,2.f,.4f}};
 const auto zero=ResolveContacts(same,10,10);
 require(zero.pairs==1 && std::isfinite(same[0].x)&&
         std::hypot(same[0].x-same[1].x,same[0].z-same[1].z)>.7f,
         "coincident centres separate deterministically");
 std::vector<Body> wall{{11,0,0,.45f}};
 ResolveContacts(wall,5,5);
 require(wall[0].x>=.45f&&wall[0].z>=.45f,"wall clamping");
 std::vector<Body> busy;
 for(int i=0;i<500;++i)
   busy.push_back({uint64_t(i+1),.2f+float(i%22)*.9f,
                    .2f+float(i/22)*.9f,.24f});
 const auto large=ResolveContacts(busy,30,30);
 require(large.maxRemainingDepth<.1f,"broadphase stable under 500 actors");
 std::vector<Body> duplicate{{1,2,2,.2f},{1,2,2,.2f}};
 const auto original=duplicate;
 reject([&]{ResolveContacts(duplicate,10,10);},"duplicate IDs rejected");
 require(duplicate[0].x==original[0].x,"invalid input never mutated");
 reject([&]{ResolveContacts(b,10,10,0);},"passes bound");
 reject([&]{std::vector<Body> invalid{{5,2,2,1.f}};ResolveContacts(invalid,10,10);},
        "radius bound");
}
}
int main(){
 try{geometry();collisions();std::cout<<"Creature geometry/gait/contact tests passed\n";return 0;}
 catch(const std::exception&e){std::cerr<<"Creature test failed: "<<e.what()<<'\n';return 1;}
}
