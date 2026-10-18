#include "Include/Bindless.hlsli"
#include "Include/Common.hlsli"

ROOT_CONSTANTS(LuminanceHistogramRootConstants, rc)

groupshared uint histogram_shared[LUMINANCE_HISTOGRAM_BIN_COUNT];

uint LuminanceToHistogramBin(float3 inColor)
{
    const float luminance = LuminanceLinear(inColor);

    if (luminance < 1e-6)
        return 0;

    const float log_luminance = saturate((log2(luminance) - rc.mMinLogLuminance) * rc.mOneOverLogLuminanceRange);
    return uint(log_luminance * (LUMINANCE_HISTOGRAM_BIN_COUNT - 2) + 1.0);
}


[numthreads(LUMINANCE_HISTOGRAM_GROUP_SIZE, LUMINANCE_HISTOGRAM_GROUP_SIZE, 1)]
void main(uint inGroupIndex : SV_GroupIndex, uint3 inThreadID : SV_DispatchThreadID)
{
    Texture2D<float4> input_texture = ResourceDescriptorHeap[rc.mInputTexture];
    RWByteAddressBuffer histogram_buffer = ResourceDescriptorHeap[rc.mHistogramBuffer];

    histogram_shared[inGroupIndex] = 0;

    GroupMemoryBarrierWithGroupSync();

    if (all(inThreadID.xy < rc.mInputSize))
    {
        const float3 color = input_texture[inThreadID.xy].rgb;
        InterlockedAdd(histogram_shared[LuminanceToHistogramBin(color)], 1);
    }

    GroupMemoryBarrierWithGroupSync();

    if (histogram_shared[inGroupIndex] > 0)
        histogram_buffer.InterlockedAdd(inGroupIndex * 4, histogram_shared[inGroupIndex]);
}
