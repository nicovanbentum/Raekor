#include "Include/Bindless.hlsli"
#include "Include/Common.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(DenoiseRootConstants, rc)

#define MAX_HISTORY_LENGTH 32.0f
#define DEPTH_TOLERANCE 0.05f
#define CLAMP_SIGMA_SCALE 1.5f
#define CLAMP_RESET_THRESHOLD 0.05f
#define CLAMP_RESET_HISTORY_LENGTH 4.0f

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
            float neighborhood_mean = 0.0f;
            float neighborhood_moment = 0.0f;

            for (int y = -1; y <= 1; y++)
            {
                for (int x = -1; x <= 1; x++)
                {
                    const float tap = input_texture[clamp(int2(threadID) + int2(x, y), int2(0, 0), int2(rc.mDispatchSize) - 1)];
                    neighborhood_mean += tap;
                    neighborhood_moment += tap * tap;
                }
            }

            neighborhood_mean /= 9.0f;
            neighborhood_moment /= 9.0f;

            const float neighborhood_sigma = sqrt(max(neighborhood_moment - neighborhood_mean * neighborhood_mean, 0.0f));
            const float clamped_history = clamp(history.x, neighborhood_mean - CLAMP_SIGMA_SCALE * neighborhood_sigma, neighborhood_mean + CLAMP_SIGMA_SCALE * neighborhood_sigma);
            const float history_variance = max(history.y - history.x * history.x, 0.0f);

            float history_length = min(history.z + 1.0f, MAX_HISTORY_LENGTH);

            if (abs(clamped_history - history.x) > CLAMP_RESET_THRESHOLD)
                history_length = min(history_length, CLAMP_RESET_HISTORY_LENGTH);

            const float alpha = 1.0f / history_length;

            result.x = lerp(clamped_history, signal, alpha);
            result.y = lerp(history_variance + clamped_history * clamped_history, signal * signal, alpha);
            result.z = history_length;
        }
    }

    output_texture[threadID] = result;
}
