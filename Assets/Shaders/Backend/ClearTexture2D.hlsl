#include "Include/Bindless.hlsli"
#include "Include/Packing.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"
#include "Include/DDGI.hlsli"

ROOT_CONSTANTS(ClearTextureRootConstants, rc)

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    RWTexture2D<float4> texture = ResourceDescriptorHeap[rc.mTexture];
    texture[threadID.xy] = rc.mClearValue;
}