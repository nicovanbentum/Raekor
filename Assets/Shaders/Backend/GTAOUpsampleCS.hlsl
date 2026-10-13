#include "Include/Bindless.hlsli"
#include "Include/Common.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(GTAOUpsampleRootConstants, rc)

#define DEPTH_SIGMA 0.05f
#define SKY_DEPTH_DELTA 1e10f

static const Texture2D<float> g_InputTexture = ResourceDescriptorHeap[rc.mInputTexture];
static const Texture2D<float> g_DepthTexture = ResourceDescriptorHeap[rc.mDepthTexture];
static const RWTexture2D<float> g_OutputTexture = ResourceDescriptorHeap[rc.mOutputTexture];


float GetViewDepth(uint2 inPixel, float inDepth)
{
    const float2 screen_uv = (float2(inPixel) + 0.5f) / float2(rc.mDispatchSize);
    return -ReconstructPosition(screen_uv, inDepth, fc.mInvProjectionMatrix).z;
}


[numthreads(8, 8, 1)]
void main(uint2 threadID : SV_DispatchThreadID)
{
    if (any(threadID >= rc.mDispatchSize))
        return;

    const float depth = g_DepthTexture[threadID];

    if (depth >= 1.0f)
    {
        g_OutputTexture[threadID] = 1.0f;
        return;
    }

    uint2 input_size;
    g_InputTexture.GetDimensions(input_size.x, input_size.y);

    const float center_depth = GetViewDepth(threadID, depth);

    const float2 input_pixel = (float2(threadID) + 0.5f) / float(1u << rc.mResolutionShift) - 0.5f;
    const int2 base_pixel = int2(floor(input_pixel));
    const float2 bilinear = input_pixel - float2(base_pixel);

    float value_sum = 0.0f;
    float weight_sum = 0.0f;
    float closest_value = 1.0f;
    float closest_delta = SKY_DEPTH_DELTA;

    for (int tap = 0; tap < 4; tap++)
    {
        const int2 offset = int2(tap & 1, tap >> 1);
        const uint2 coord = uint2(clamp(base_pixel + offset, int2(0, 0), int2(input_size) - 1));
        const uint2 tap_pixel = min(coord << rc.mResolutionShift, rc.mDispatchSize - 1);

        const float tap_depth = g_DepthTexture[tap_pixel];
        const float tap_value = g_InputTexture[coord];
        const float delta = tap_depth >= 1.0f ? SKY_DEPTH_DELTA : abs(GetViewDepth(tap_pixel, tap_depth) - center_depth);

        if (delta < closest_delta)
        {
            closest_delta = delta;
            closest_value = tap_value;
        }

        const float2 weights = lerp(1.0f - bilinear, bilinear, float2(offset));
        const float weight = weights.x * weights.y * exp(-delta / (DEPTH_SIGMA * center_depth));

        value_sum += tap_value * weight;
        weight_sum += weight;
    }

    g_OutputTexture[threadID] = weight_sum > 1e-3f ? value_sum / weight_sum : closest_value;
}
