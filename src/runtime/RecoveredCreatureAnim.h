#pragma once
#include <array>
#include <cstdint>

namespace Spore::Creature::Recovered {

// Portable extraction of actual migrated Spore animation functions from
// src/game/SPCreatureAnim/animation_version.cpp; independently testable
// without the retail binary's Windows/EA/RenderWare runtime.
//
// Retail address 0x009A8170: scatter packed rank-ordered bits into sparse
// animation/property bit positions. Equivalent to a bounded software PDEP.
uint32_t DepositAnimationBits(uint32_t allowedMask,uint32_t packedBits);

// Retail address 0x009A81B0:
// nSPCreatureAnim::converters::ConvertEvent_v11_v12
// Converted event uses 24 dwords. Source v11 has 12 dwords; retail function
// clears source[10] as a side effect. This portable wrapper preserves it.
// Unassigned destination dwords are initialized to zero.
std::array<uint32_t,24> ConvertAnimationEventV11ToV12(
    std::array<uint32_t,12>& source);

} // namespace Spore::Creature::Recovered
