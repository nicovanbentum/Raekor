#ifndef BINDLESS_HLSLI
#define BINDLESS_HLSLI

#include "shared.h"
#include "common.hlsli"

SRV0(FrameConstantsBuffer)
SRV1(PassConstantsBuffer)

FrameConstants gGetFrameConstants() {
    return FrameConstantsBuffer.Load<FrameConstants>(0);
}

template<typename T>
T gGetPassConstants(uint inOffset) {
    return PassConstantsBuffer.Load<T>(inOffset);
}

#endif // BINDLESS_HLSLI