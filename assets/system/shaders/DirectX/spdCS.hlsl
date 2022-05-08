#include "include/common.hlsli"

struct RootConstants {
    uint mips;
    uint numWorkGroups;
    uint2 workGroupoffset;
    uint globalAtomicIndex;
    uint destImageIndices[13];
};

ROOT_CONSTANTS(RootConstants, rc)

globallycoherent RWStructuredBuffer<uint> globalAtomic = ResourceDescriptorHeap[rc.globalAtomicIndex];

#define A_GPU
#define A_HLSL
#include "include/ffx_a.h"

groupshared AU1 spdCounter;

groupshared AF1 spdIntermediateR[16][16];
groupshared AF1 spdIntermediateG[16][16];
groupshared AF1 spdIntermediateB[16][16];
groupshared AF1 spdIntermediateA[16][16];

AF4 SpdLoadSourceImage(AF2 tex, AU1 slice) {
    globallycoherent RWTexture2D<float4> imgDst = ResourceDescriptorHeap[rc.destImageIndices[0]];
    return imgDst[float3(tex, slice)];
}

AF4 SpdLoad(ASU2 tex, AU1 slice) {
    globallycoherent RWTexture2D<float4> imgDst = ResourceDescriptorHeap[rc.destImageIndices[6]];
    return imgDst[uint3(tex, slice)];
}

void SpdStore(ASU2 pix, AF4 outValue, AU1 mip, AU1 slice) {
    globallycoherent RWTexture2D<float4> imgDst = ResourceDescriptorHeap[rc.destImageIndices[mip]];
    imgDst[pix] = outValue;
}

void SpdIncreaseAtomicCounter(AU1 slice) {
    InterlockedAdd(globalAtomic[0], 1, spdCounter);
}

AU1 SpdGetAtomicCounter() {
    return spdCounter;
}

void SpdResetAtomicCounter(AU1 slice) {
    globalAtomic[0] = 0;
}

AF4 SpdLoadIntermediate(AU1 x, AU1 y) {
    return AF4(
    spdIntermediateR[x][y],
    spdIntermediateG[x][y],
    spdIntermediateB[x][y],
    spdIntermediateA[x][y]);
}

void SpdStoreIntermediate(AU1 x, AU1 y, AF4 value) {
    spdIntermediateR[x][y] = value.x;
    spdIntermediateG[x][y] = value.y;
    spdIntermediateB[x][y] = value.z;
    spdIntermediateA[x][y] = value.w;
}

AF4 SpdReduce4(AF4 v0, AF4 v1, AF4 v2, AF4 v3) {
    return (v0+v1+v2+v3)*0.25;
}

#include "include/ffx_spd.h"

[numthreads(256, 1, 1)]
void main(uint3 group_id, uint group_index : SV_GroupIndex) {
    SpdDownsample(group_id.xy, group_index, rc.mips, rc.numWorkGroups, 0);
}