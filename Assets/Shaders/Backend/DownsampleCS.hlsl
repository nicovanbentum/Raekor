#include "Include/Bindless.hlsli"
#include "Include/Common.hlsli"

ROOT_CONSTANTS(SpdRootConstants, rc)

#define A_GPU
#define A_HLSL
#include "Include/Ffx_A.hlsli"

groupshared AU1 spdCounter;

groupshared AF1 spdIntermediateR[16][16];
groupshared AF1 spdIntermediateG[16][16];
groupshared AF1 spdIntermediateB[16][16];
groupshared AF1 spdIntermediateA[16][16];

static const RWTexture2DArray<float4> TextureMip0 = ResourceDescriptorHeap[rc.mTextureMip0];
static const globallycoherent RWTexture2DArray<float4> TextureMip6 = ResourceDescriptorHeap[rc.mTextureMip6];
static const globallycoherent RWStructuredBuffer<uint> GlobalAtomicBuffer = ResourceDescriptorHeap[rc.mGlobalAtomicBuffer];

AF4 SpdLoadSourceImage(ASU2 tex, AU1 slice) 
{
    return TextureMip0[uint3(tex, slice)];
}

AF4 SpdLoad(ASU2 tex, AU1 slice) 
{
    return TextureMip6[uint3(tex, slice)];
}

void SpdStore(ASU2 pix, AF4 outValue, AU1 mip, AU1 slice) 
{
    if (mip == 5) 
    {
        TextureMip6[uint3(pix, slice)] = outValue;
        return;
    }
    
    uint bindless_index = -1;
    
    switch (mip) 
    {
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
    
    RWTexture2DArray<float4> texture = ResourceDescriptorHeap[NonUniformResourceIndex(bindless_index)];
    texture[uint3(pix, slice)] = outValue;
}

void SpdIncreaseAtomicCounter(AU1 slice) 
{
    InterlockedAdd(GlobalAtomicBuffer[0], 1, spdCounter);
}

AU1 SpdGetAtomicCounter() 
{
    return spdCounter;
}

void SpdResetAtomicCounter(AU1 slice) 
{
    GlobalAtomicBuffer[0] = 0;
}

AF4 SpdLoadIntermediate(AU1 x, AU1 y) 
{
    return AF4(
    spdIntermediateR[x][y],
    spdIntermediateG[x][y],
    spdIntermediateB[x][y],
    spdIntermediateA[x][y]);
}

void SpdStoreIntermediate(AU1 x, AU1 y, AF4 value) 
{
    spdIntermediateR[x][y] = value.x;
    spdIntermediateG[x][y] = value.y;
    spdIntermediateB[x][y] = value.z;
    spdIntermediateA[x][y] = value.w;
}

AF4 SpdReduce4(AF4 v0, AF4 v1, AF4 v2, AF4 v3) 
{
    return (v0 + v1 + v2 + v3) * 0.25;
}

#include "include/Ffx_SPD.hlsli"

[numthreads(256, 1, 1)]
void main(uint3 group_id : SV_GroupID, uint group_index : SV_GroupIndex) 
{
    SpdDownsample(group_id.xy, group_index, rc.mNrOfMips, rc.mNrOfWorkGroups, group_id.z);
}