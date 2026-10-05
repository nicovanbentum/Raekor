#include "Include/Bindless.hlsli"
#include "Include/Common.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(DenoiseRootConstants, rc)

#define MAX_HISTORY_LENGTH 32.0f
#define DEPTH_TOLERANCE 0.05f

[numthreads(8, 8, 1)]
void main(uint2 threadID : SV_DispatchThreadID)
{
    if (any(threadID >= rc.mDispatchSize))
        return;

    Texture2D<float> input_texture = ResourceDescriptorHeap[rc.mInputTexture];
    Texture2D<float4> history_texture = ResourceDescriptorHeap[rc.mHistoryTexture];
    Texture2D<float> depth_texture = ResourceDescriptorHeap[rc.mDepthTexture];
    Texture2D<float2> velocity_texture = ResourceDescriptorHeap[rc.mVelocityTexture];
    RWTexture2D<float4> output_texture = ResourceDescriptorHeap[rc.mOutputTexture];

    const float signal = input_texture[threadID];
    const float depth = depth_texture[threadID];

    if (depth >= 1.0f)
    {
        output_texture[threadID] = float4(signal, signal * signal, 1.0f, 0.0f);
        return;
    }

    const float2 screen_uv = (float2(threadID) + 0.5f) / float2(rc.mDispatchSize);
    const float3 ws_position = ReconstructWorldPosition(screen_uv, depth, fc.mInvViewProjectionMatrix);

    const float view_depth = mul(fc.mViewProjectionMatrix, float4(ws_position, 1.0f)).w;
    const float prev_view_depth = mul(fc.mPrevViewProjectionMatrix, float4(ws_position, 1.0f)).w;

    const float2 prev_screen_uv = screen_uv - velocity_texture[threadID];

    float4 result = float4(signal, signal * signal, 1.0f, view_depth);

    if (fc.mFrameCounter >= 2 && all(prev_screen_uv >= 0.0f) && all(prev_screen_uv <= 1.0f))
    {
        const float4 history = history_texture.SampleLevel(SamplerPointClamp, prev_screen_uv, 0);

        if (history.w > 0.0f && abs(history.w - prev_view_depth) < DEPTH_TOLERANCE * prev_view_depth)
        {
            const float history_length = min(history.z + 1.0f, MAX_HISTORY_LENGTH);
            const float alpha = 1.0f / history_length;

            result.x = lerp(history.x, signal, alpha);
            result.y = lerp(history.y, signal * signal, alpha);
            result.z = history_length;
        }
    }

    output_texture[threadID] = result;
}
