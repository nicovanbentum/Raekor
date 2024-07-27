#include "Include/Bindless.hlsli"
#include "Include/Common.hlsli"

ROOT_CONSTANTS(ClearBufferRootConstants, rc)

[numthreads(64, 1, 1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    RWBuffer<float4> buffer = ResourceDescriptorHeap[rc.mBuffer];
    buffer[threadID.x] = rc.mClearValue;
}