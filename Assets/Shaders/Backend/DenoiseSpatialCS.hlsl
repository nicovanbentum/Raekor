#include "Include/Bindless.hlsli"
#include "Include/Common.hlsli"
#include "Include/Packing.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(DenoiseRootConstants, rc)

#define DEPTH_SIGMA 0.02f
#define NORMAL_POWER 32.0f
#define VALUE_SIGMA 4.0f

static const float sKernel[3] = { 3.0f / 8.0f, 1.0f / 4.0f, 1.0f / 16.0f };

[numthreads(8, 8, 1)]
void main(uint2 threadID : SV_DispatchThreadID)
{
    if (any(threadID >= rc.mDispatchSize))
        return;

    Texture2D<float4> input_texture = ResourceDescriptorHeap[rc.mInputTexture];
    Texture2D<float4> gbuffer_texture = ResourceDescriptorHeap[rc.mGBufferTexture];
    RWTexture2D<float4> output_texture = ResourceDescriptorHeap[rc.mOutputTexture];

    const float4 center = input_texture[threadID];

    if (center.w <= 0.0f)
    {
        output_texture[threadID] = center;
        return;
    }

    const float3 center_normal = UnpackNormal(asuint(gbuffer_texture[threadID]));

    const float variance = max(center.y - center.x * center.x, 0.0f) + 1.0f / (center.z * center.z * 16.0f);
    const float value_sigma = VALUE_SIGMA * sqrt(variance) + 1e-4f;

    float value_sum = 0.0f;
    float weight_sum = 0.0f;

    for (int y = -2; y <= 2; y++)
    {
        for (int x = -2; x <= 2; x++)
        {
            const int2 coord = int2(threadID) + int2(x, y) * int(rc.mStepSize);

            if (any(coord < 0) || any(coord >= int2(rc.mDispatchSize)))
                continue;

            const float4 tap = input_texture[coord];

            if (tap.w <= 0.0f)
                continue;

            const float3 tap_normal = UnpackNormal(asuint(gbuffer_texture[coord]));

            const float depth_weight = exp(-abs(center.w - tap.w) / (DEPTH_SIGMA * center.w * float(rc.mStepSize) * length(float2(x, y)) + 1e-4f));
            const float normal_weight = pow(saturate(dot(center_normal, tap_normal)), NORMAL_POWER);
            const float value_weight = exp(-abs(center.x - tap.x) / value_sigma);

            const float weight = sKernel[abs(x)] * sKernel[abs(y)] * depth_weight * normal_weight * value_weight;

            value_sum += tap.x * weight;
            weight_sum += weight;
        }
    }

    output_texture[threadID] = float4(value_sum / max(weight_sum, 1e-5f), center.yzw);
}
