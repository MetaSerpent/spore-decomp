#include "runtime/TerraformField.h"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using Spore::Terrain::TerraformField;
namespace {
void Expect(bool v,const char* msg){if(!v)throw std::runtime_error(msg);}
template<typename F> void Reject(F f,const char* msg){
 bool rejected=false;
 try{f();}catch(const std::invalid_argument&){rejected=true;}
 Expect(rejected,msg);
}
void TestBrushAndBounds(){
 TerraformField world(33,33,.4f);
 const auto base=world.Checksum();
 Expect(std::abs(world.Mean()-.4)<.00001,"initial mean");
 Expect(world.PlanetType(.75f)==1,"initial planet type");
 const auto change=world.Apply(.5f,.5f,.25f,.6f);
 Expect(change.affected>1 && change.affected<33*33,"brush affects finite area");
 Expect(change.appliedDelta>0,"brush raises terrain");
 Expect(std::abs(world.At(16,16)-1.f)<1e-6,"brush centre maximum");
 Expect(world.At(0,0)==.4f,"unaffected corner");
 Expect(world.Checksum()!=base,"checksum changes");
 Expect(world.Mean()>.4,"mean rises");
 for(unsigned y=0;y<33;y++)for(unsigned x=0;x<33;x++){
   Expect(std::abs(world.At(x,y)-world.At(32-x,32-y))<1e-6f,"radial symmetry");
   Expect(world.At(x,y)>=0&&world.At(x,y)<=1,"clamped terrain");
 }
 const auto unchanged=world.Checksum();
 Expect(world.Apply(.5f,.5f,.5f,0.f).affected==0,"zero brush noop");
 Expect(world.Checksum()==unchanged,"zero brush does not mutate");
 const auto lower=world.Apply(.5f,.5f,.25f,-1.f);
 Expect(lower.affected>0 && lower.appliedDelta<0,"negative brush lowers terrain");
 Expect(world.At(16,16)==0.f,"negative brush saturates at zero");
}
void TestSnapshotAndDeterminism(){
 TerraformField a(28,19,.52f),b(28,19,.52f);
 std::uint64_t state=0xA3D125A99123ULL;
 for(unsigned i=0;i<3000;i++){
   state=state*UINT64_C(6364136223846793005)+UINT64_C(1442695040888963407);
   const float x=float((state>>16)&65535)/65535.f;
   const float y=float((state>>32)&65535)/65535.f;
   const float delta=(i%2?-.21f:.13f);
   const auto ba=a.Apply(x,y,.12f,delta);
   const auto bb=b.Apply(x,y,.12f,delta);
   Expect(ba.affected==bb.affected&&ba.appliedDelta==bb.appliedDelta,"brush determinism");
 }
 Expect(a.Checksum()==b.Checksum(),"identical state hashes");
 auto encoded=a.Snapshot();
 auto restored=TerraformField::Restore(encoded);
 Expect(restored.Checksum()==a.Checksum(),"snapshot checksum roundtrip");
 Expect(restored.Snapshot()==encoded,"canonical snapshots are stable");
 const auto brush1=a.Apply(.2f,.7f,.11f,.31f);
 const auto brush2=restored.Apply(.2f,.7f,.11f,.31f);
 Expect(brush1.affected==brush2.affected,"deterministic after restore");
 Expect(a.Checksum()==restored.Checksum(),"continued simulation matches");
 Reject([&]{(void)TerraformField::Restore(encoded.substr(0,encoded.size()/2));},"truncated snapshot rejected");
 Reject([&]{(void)TerraformField::Restore(encoded+"garbage");},"trailing snapshot rejected");
 Reject([&]{(void)TerraformField::Restore("SPORE_TERRAFORM_FIELD 8\n2 2\n0 0 0 0\n");},"version rejected");
 Reject([&]{(void)TerraformField::Restore("SPORE_TERRAFORM_FIELD 1\n2 2\n0 0 -1 0\n");},"invalid value rejected");
}
void TestInputs(){
 Reject([]{TerraformField w(1,5);},"minimum width");
 Reject([]{TerraformField w(3,1);},"minimum height");
 Reject([]{TerraformField w(2000,2000);},"limited allocation");
 Reject([]{TerraformField w(3,3,-1);},"negative initial value");
 TerraformField f(3,3,.5f);
 Reject([&]{f.Apply(.5f,.5f,0,.4f);},"zero radius");
 Reject([&]{f.Apply(1.1f,.5f,.2f,.4f);},"out of range centre");
 Reject([&]{f.Apply(.5f,.5f,.2f,2.f);},"delta out of range");
 Expect(f.At(0,0)==.5f,"invalid brush leaves field intact");
 bool threw=false;
 try{(void)f.At(3,0);}catch(const std::out_of_range&){threw=true;}
 Expect(threw,"out of bounds sample rejected");
}
}
int main(){
 try{TestBrushAndBounds();TestSnapshotAndDeterminism();TestInputs();std::cout<<"Terraform field tests passed\n";return 0;}
 catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
