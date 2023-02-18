#include "include/bindless.hlsli"
#include "include/common.hlsli"

ROOT_CONSTANTS(SpdRootConstants, rc)

#define A_GPU
#define A_HLSL
#include "include/ffx_a.h"

groupshared AU1 spdCounter;

groupshared AF1 spdIntermediateR[16][16];
groupshared AF1 spdIntermediateG[16][16];
groupshared AF1 spdIntermediateB[16][16];
groupshared AF1 spdIntermediateA[16][16];



AF4 SpdLoadSourceImage(ASU2 tex, AU1 slice) {
    RWTexture2D<float4> texture_mip0 = ResourceDescriptorHeap[rc.mTextureMip0];
    return texture_mip0[tex];
}

AF4 SpdLoad(ASU2 tex, AU1 slice) {
    globallycoherent RWTexture2D<float4> texture_mip = ResourceDescriptorHeap[rc.mTextureMip6];
    return texture_mip[tex];
}

void SpdStore(ASU2 pix, AF4 outValue, AU1 mip, AU1 slice) {
    if (mip == 5) {
        globallycoherent RWTexture2D<float4> texture = ResourceDescriptorHeap[rc.mTextureMip6];
        texture[pix] = outValue;
        return;
    }
    
    uint bindless_index = -1;
    
    switch (mip) {
        case 0:  bindless_index = rc.mTextureMip1;  break;
        case 1:  bindless_index = rc.mTextureMip2;  break;
        case 2:  bindless_index = rc.mTextureMip3;  break;
        case 3:  bindless_index = rc.mTextureMip4;  break;
        case 4:  bindless_index = rc.mTextureMip5;  break;
        case 5:  bindless_index = rc.mTextureMip6;  break;
        case 6:  bindless_index = rc.mTextureMip7;  break;
        case 7:  bindless_index = rc.mTextureMip8;  break;
        case 8:  bindless_index = rc.mTextureMip9;  break;
        case 9:  bindless_index = rc.mTextureMip10; break;
        case 10: bindless_index = rc.mTextureMip11; break;
        case 11: bindless_index = rc.mTextureMip12; break;
        case 12: bindless_index = rc.mTextureMip13; break;
    }
    
    RWTexture2D<float4> texture = ResourceDescriptorHeap[NonUniformResourceIndex(bindless_index)];
    texture[pix] = outValue;
}

void SpdIncreaseAtomicCounter(AU1 slice) {
    globallycoherent RWStructuredBuffer<uint> globalAtomic = ResourceDescriptorHeap[rc.mGlobalAtomicBuffer];
    InterlockedAdd(globalAtomic[0], 1, spdCounter);
}

AU1 SpdGetAtomicCounter() {
    return spdCounter;
}

void SpdResetAtomicCounter(AU1 slice) {
    globallycoherent RWStructuredBuffer<uint> globalAtomic = ResourceDescriptorHeap[rc.mGlobalAtomicBuffer];
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
    return (v0 + v1 + v2 + v3) * 0.25;
}

#include "include/ffx_spd.h"

[numthreads(256, 1, 1)]
void main(uint3 group_id : SV_GroupID, uint group_index : SV_GroupIndex) {
    SpdDownsample(group_id.xy, group_index, rc.mNrOfMips, rc.mNrOfWorkGroups, 0);
}