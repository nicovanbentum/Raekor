#include "Include/Bindless.hlsli"
#include "Include/Packing.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"
#include "Include/DDGI.hlsli"

ROOT_CONSTANTS(ClearTextureRootConstants, rc)

[numthreads(4, 4, 4)]
void main(uint3 threadID : SV_DispatchThreadID) {
    RWTexture3D<float4> texture = ResourceDescriptorHeap[rc.mTexture];
    texture[threadID.xyz] = rc.mClearValue;
}