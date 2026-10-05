#include "Include/Shared.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"
#include "Include/Bindless.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(GTAORootConstants, rc)

#define GTAO_SECTOR_COUNT 32u
#define GTAO_MAX_RADIUS_PIXELS 256.0f
#define HALF_PI (M_PI * 0.5f)

static const Texture2D<float> g_DepthTexture = ResourceDescriptorHeap[rc.mDepthTexture];
static const Texture2D<uint4> g_GBufferTexture = ResourceDescriptorHeap[rc.mGBufferTexture];
static const RWTexture2D<float> g_OutputTexture = ResourceDescriptorHeap[rc.mOutputTexture];


float3 GetViewPosition(float2 inUV, float inDepth)
{
    return ReconstructPosition(inUV, inDepth, fc.mInvProjectionMatrix);
}


uint UpdateSectors(float inMinHorizon, float inMaxHorizon, uint inBitmask)
{
    const uint start = uint(inMinHorizon * GTAO_SECTOR_COUNT);
    const uint angle = uint(ceil(saturate(inMaxHorizon - inMinHorizon) * GTAO_SECTOR_COUNT));
    const uint bits = angle > 0u ? (0xFFFFFFFFu >> (GTAO_SECTOR_COUNT - angle)) : 0u;

    return start < GTAO_SECTOR_COUNT ? inBitmask | (bits << start) : inBitmask;
}


float IntegrateCosineArc(float inAngle, float inNormalAngle)
{
    const float sin_n = sin(inNormalAngle);
    const float cos_n = cos(inNormalAngle);

    if (inAngle >= 0.0f)
        return -0.25f * cos(2.0f * inAngle - inNormalAngle) + 0.5f * inAngle * sin_n + 0.25f * cos_n;
    else
        return 0.25f * cos(2.0f * inAngle - inNormalAngle) - 0.5f * inAngle * sin_n - 0.25f * cos_n;
}


float GetSectorAngle(uint inSector, float inNormalAngle)
{
    return inNormalAngle - HALF_PI + float(inSector) * (M_PI / float(GTAO_SECTOR_COUNT));
}


[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    if (any(threadID.xy >= rc.mDispatchSize))
        return;

    const float depth = g_DepthTexture[threadID.xy];

    if (depth >= 1.0f)
    {
        g_OutputTexture[threadID.xy] = 1.0f;
        return;
    }

    const float2 texel_size = 1.0f / float2(rc.mDispatchSize);
    const float2 screen_uv = (float2(threadID.xy) + 0.5f) * texel_size;

    const float3 vs_position = GetViewPosition(screen_uv, depth);
    const float3 vs_view = normalize(-vs_position);

    const float3 ws_normal = UnpackNormal(g_GBufferTexture[threadID.xy]);
    const float3 vs_normal = normalize(mul((float3x3)fc.mViewMatrix, ws_normal));

    const float radius_pixels = min(rc.mRadius * fc.mProjectionMatrix[1][1] * 0.5f * float(rc.mDispatchSize.y) / -vs_position.z, GTAO_MAX_RADIUS_PIXELS);

    if (radius_pixels < 1.0f)
    {
        g_OutputTexture[threadID.xy] = 1.0f;
        return;
    }

    const float4 blue_noise = SampleBlueNoise(threadID.xy, fc.mFrameCounter);

    float visibility = 0.0f;
    float weight_sum = 0.0f;

    for (uint slice_index = 0; slice_index < rc.mSliceCount; slice_index++)
    {
        const float phi = (float(slice_index) + blue_noise.x) * (M_PI / float(rc.mSliceCount));
        const float2 omega = float2(cos(phi), sin(phi));

        const float3 direction = float3(omega.x, omega.y, 0.0f);
        const float3 ortho_direction = direction - dot(direction, vs_view) * vs_view;
        const float3 axis = normalize(cross(direction, vs_view));

        const float3 projected_normal = vs_normal - axis * dot(vs_normal, axis);
        const float projected_length = length(projected_normal);

        if (projected_length < 1e-4f)
            continue;

        const float sign_n = dot(ortho_direction, projected_normal) >= 0.0f ? 1.0f : -1.0f;
        const float cos_n = saturate(dot(projected_normal, vs_view) / projected_length);
        const float n = sign_n * acos(cos_n);

        const float2 uv_direction = float2(omega.x, -omega.y) * texel_size;

        uint bitmask = 0u;

        for (uint side = 0; side < 2; side++)
        {
            const float side_sign = side == 0 ? -1.0f : 1.0f;

            for (uint step_index = 0; step_index < rc.mStepCount; step_index++)
            {
                float t = (float(step_index) + blue_noise.y) / float(rc.mStepCount);
                t *= t;

                const float2 sample_uv = screen_uv + side_sign * uv_direction * max(t * radius_pixels, 1.0f);

                if (any(sample_uv < 0.0f) || any(sample_uv > 1.0f))
                    break;

                const float sample_depth = g_DepthTexture.SampleLevel(SamplerPointClamp, sample_uv, 0);

                if (sample_depth >= 1.0f)
                    continue;

                const float3 sample_delta = GetViewPosition(sample_uv, sample_depth) - vs_position;
                const float sample_distance = length(sample_delta);

                if (sample_distance < 1e-4f)
                    continue;

                const float3 front_direction = sample_delta / sample_distance;
                const float3 back_direction = normalize(sample_delta - vs_view * rc.mThickness);

                float2 horizons = acos(clamp(float2(dot(front_direction, vs_view), dot(back_direction, vs_view)), -1.0f, 1.0f));
                horizons = saturate((side_sign * horizons - n + HALF_PI) / M_PI);
                horizons = side_sign > 0.0f ? horizons : horizons.yx;

                bitmask = UpdateSectors(horizons.x, horizons.y, bitmask);
            }
        }

        const float total = IntegrateCosineArc(n + HALF_PI, n) - IntegrateCosineArc(n - HALF_PI, n);

        float occluded = 0.0f;
        uint remaining = bitmask;

        while (remaining != 0u)
        {
            const uint run_start = firstbitlow(remaining);
            const uint run_gaps = ~remaining & (0xFFFFFFFFu << run_start);
            const uint run_end = run_gaps != 0u ? firstbitlow(run_gaps) : GTAO_SECTOR_COUNT;

            occluded += IntegrateCosineArc(GetSectorAngle(run_end, n), n) - IntegrateCosineArc(GetSectorAngle(run_start, n), n);

            remaining = run_end < GTAO_SECTOR_COUNT ? remaining & (0xFFFFFFFFu << run_end) : 0u;
        }

        visibility += projected_length * saturate(1.0f - occluded / max(total, 1e-5f));
        weight_sum += projected_length;
    }

    visibility = weight_sum > 0.0f ? visibility / weight_sum : 1.0f;

    g_OutputTexture[threadID.xy] = pow(visibility, rc.mPower);
}
