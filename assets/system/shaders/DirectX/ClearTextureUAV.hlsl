#include "include/bindless.hlsli"
#include "include/packing.hlsli"
#include "include/common.hlsli"
#include "include/random.hlsli"
#include "include/ddgi.hlsli"

ROOT_CONSTANTS(ClearTextureRootConstants, rc)

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    RWTexture2D<float4> texture = ResourceDescriptorHeap[rc.mTexture];
    texture[threadID.xy] = rc.mClearValue;
}