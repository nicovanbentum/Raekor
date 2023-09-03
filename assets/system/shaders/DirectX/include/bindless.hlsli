#ifndef BINDLESS_HLSLI
#define BINDLESS_HLSLI

#include "shared.h"

 //#define GLOBAL_ROOT_SIGNATURE "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED )," \
 //                            "RootConstants(num32BitConstants=32, b0), " \
 //                            "StaticSampler(s0, addressU = TEXTURE_ADDRESS_CLAMP, filter = FILTER_ANISOTROPIC)"

#define ROOT_CONSTANTS(T, name) ConstantBuffer<T> name : register(b0, space0);

#define CBV0(T, name) ConstantBuffer<T> name : register(b1, space0);
#define CBV1(T, name) ConstantBuffer<T> name : register(b2, space0);
#define CBV2(T, name) ConstantBuffer<T> name : register(b3, space0);
#define CBV3(T, name) ConstantBuffer<T> name : register(b4, space0);

#define SRV0(name) ByteAddressBuffer name : register(t0, space0);
#define SRV1(name) ByteAddressBuffer name : register(t1, space0);
#define SRV2(name) ByteAddressBuffer name : register(t2, space0);
#define SRV3(name) ByteAddressBuffer name : register(t3, space0);

#define UAV0(T, name) RWStructuredBuffer<T> name : register(u0, space0);
#define UAV1(T, name) RWStructuredBuffer<T> name : register(u1, space0);
#define UAV2(T, name) RWStructuredBuffer<T> name : register(u2, space0);
#define UAV3(T, name) RWStructuredBuffer<T> name : register(b3, space0);

SamplerState SamplerPointWrapNoMips   : register(s0);
SamplerState SamplerPointWrap         : register(s1);
SamplerState SamplerPointClamp        : register(s2);
SamplerState SamplerLinearWrap        : register(s3);
SamplerState SamplerLinearClamp       : register(s4);
SamplerState SamplerAnisoWrap         : register(s5);
SamplerState SamplerAnisoClamp        : register(s6);

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