#include "runtime/RecoveredCreatureAnim.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>

using namespace Spore::Creature::Recovered;
namespace {
void Require(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}

uint32_t IndependentDeposit(uint32_t allowed,uint32_t packed) {
 uint32_t result=0;
 unsigned rank=0;
 for(unsigned bit=0;bit<32;++bit) {
  const uint32_t mask=(uint32_t(1)<<bit);
  if(!(allowed&mask))continue;
  if(packed&(uint32_t(1)<<rank))result|=mask;
  ++rank;
 }
 return result;
}
void TestRankedBits() {
 Require(DepositAnimationBits(0xA0u,0b01u)==0x20u,
         "deposit first ranked bit into first mask position");
 Require(DepositAnimationBits(0xA0u,0b10u)==0x80u,
         "deposit second ranked bit into second position");
 Require(DepositAnimationBits(0xB4u,0b1001u)==0x84u,
         "deposit sparse ranked bits");
 Require(DepositAnimationBits(0xffffffffu,0xAABBCCDDu)==0xAABBCCDDu,
         "all mask bits retain packed bit pattern");
 Require(DepositAnimationBits(0u,0xffffffffu)==0u,"zero mask");
 Require(DepositAnimationBits(0xffffffffu,0u)==0u,"zero source");
 uint32_t rng=0x83A12F1Cu;
 for(unsigned i=0;i<20000;++i){
  rng=rng*UINT32_C(1664525)+UINT32_C(1013904223);
  const uint32_t mask=rng;
  rng=rng*UINT32_C(1664525)+UINT32_C(1013904223);
  const uint32_t packed=rng;
  Require(DepositAnimationBits(mask,packed)==IndependentDeposit(mask,packed),
          "recovered deposit function matches independent reference");
 }
}
void TestV11ToV12() {
 std::array<uint32_t,12> src{};
 for(unsigned i=0;i<12;++i)src[i]=0xA0010000u+i;
 src[0]=0x800u;
 src[1]=0xFFFFFFFFu;
 src[2]=0xFFFFFFFFu;
 src[10]=0x12345678u;
 const auto copy=src;
 const auto dst=ConvertAnimationEventV11ToV12(src);
 Require(src[10]==0u && src[9]==copy[9] && src[11]==copy[11],
         "retail converter mutates source word 10 only");
 Require(dst[0]==0x5800u,
         "source flag migration (0x0800 implies 0x4800)");
 Require(dst[1]==0xFFFEFFFFu &&
         dst[4]==0xFFFEFFFFu&&dst[7]==0xFFFEFFFFu,
         "attribute flags duplicated into three old stage slots");
 Require(dst[2]==copy[3]&&dst[5]==copy[3]&&dst[8]==copy[3],
         "first timing field replicated");
 Require(dst[3]==copy[4]&&dst[6]==copy[4]&&dst[9]==copy[4],
         "second timing field replicated");
 Require(dst[10]==0xFFFEFFFFu,"event bit field migration");
 for(int i=11;i<=16;++i)
  Require(dst[size_t(i)]==copy[size_t(i-6)],
          "event fields copied to migrated indices");
 Require(dst[17]==copy[11]&&dst[18]==0,
         "tail event migrated, unused field initialized");
 for(unsigned i=19;i<=23;++i)Require(dst[i]==0u,"new fields zero-initialized");

 // An empty event remains entirely empty, without introducing flags.
 std::array<uint32_t,12> zero{};
 const auto empty=ConvertAnimationEventV11ToV12(zero);
 for(const auto field:empty)Require(field==0u,"empty event preserves zero flags");
}
}
int main(){
 try{
  TestRankedBits();TestV11ToV12();
  std::cout<<"Migrated creature animation functions passed portable regression\n";
  return 0;
 }catch(const std::exception& e){
  std::cerr<<"Recovered creature animation regression failed: "<<e.what()<<'\n';
  return 1;
 }
}
