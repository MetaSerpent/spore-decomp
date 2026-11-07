#include "runtime/RecoveredCreatureAnim.h"

namespace Spore::Creature::Recovered {

// @ 0x009A8170
uint32_t DepositAnimationBits(uint32_t mask,uint32_t packed) {
  uint32_t result=0;
  // The original loops through bits until either operand is zero. A bounded
  // 32-bit loop gives the same result for valid 32-bit inputs, and cannot
  // become non-terminating when the original moving bit wraps to zero.
  for(unsigned rank=0;rank<32&&mask!=0&&packed!=0;++rank) {
    const uint32_t lowest=mask&(~mask+1u);
    if((packed&1u)!=0)result|=lowest;
    mask&=mask-1u;
    packed>>=1u;
  }
  return result;
}

// @ 0x009A81B0
std::array<uint32_t,24> ConvertAnimationEventV11ToV12(
    std::array<uint32_t,12>& src) {
  std::array<uint32_t,24> out{};
  out[0]=src[0];
  auto value=src[2]&0xFFFEEFFFu;
  out[10]=value;
  if((src[2]&0x1000u)!=0)out[10]=value|0x1000u;
  out[11]=src[5];out[12]=src[6];out[13]=src[7];
  out[14]=src[8];out[15]=src[9];out[16]=src[10];
  src[10]=0u;
  out[17]=src[11];
  out[19]=0;out[20]=0;out[21]=0;out[22]=0;out[23]=0;
  out[0]&=0xFFFFA7FFu;
  if((src[0]&0x800u)!=0)out[0]|=0x4800u;
  if((src[0]&0x1800u)!=0)out[0]|=0x1000u;
  value=src[1]&0xFFFEEFFFu;
  if((src[1]&0x1000u)!=0)value|=0x1000u;
  out[1]=value;out[4]=value;out[7]=value;
  out[2]=src[3];out[5]=src[3];out[8]=src[3];
  out[3]=src[4];out[6]=src[4];out[9]=src[4];
  return out;
}

} // namespace Spore::Creature::Recovered
