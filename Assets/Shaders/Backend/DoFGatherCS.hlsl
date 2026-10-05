#include "Include/Bindless.hlsli"
#include "Include/DepthOfField.hlsli"

ROOT_CONSTANTS(DepthOfFieldRootConstants, rc)

[numthreads(8, 8, 1)]
void main(uint2 threadID : SV_DispatchThreadID)
{
    if (any(threadID >= rc.mDispatchSize))
        return;

    Texture2D<float4> half_res_texture = ResourceDescriptorHeap[rc.mHalfResTexture];
    Texture2D<float> tile_texture = ResourceDescriptorHeap[rc.mTileTexture];
    RWTexture2D<float4> blur_texture = ResourceDescriptorHeap[rc.mBlurTexture];

    uint2 tile_count;
    tile_texture.GetDimensions(tile_count.x, tile_count.y);

    const int2 tile = int2(threadID / DOF_TILE_SIZE);
    float max_coc = 0.0f;

    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
            max_coc = max(max_coc, tile_texture[clamp(tile + int2(x, y), 0, int2(tile_count) - 1)]);
    }

    const float4 center = half_res_texture[threadID];

    if (max_coc < 0.5f)
    {
        blur_texture[threadID] = float4(center.rgb, 0.0f);
        return;
    }

    const float2 texel_size = 1.0f / float2(rc.mDispatchSize);
    const float2 center_uv = (float2(threadID) + 0.5f) * texel_size;

    const float center_coc = center.a;
    const float center_size = abs(center_coc);

    float4 color = float4(center.rgb, 1.0f);
    float near_weight = 0.0f;

    for (uint i = 0; i < DOF_SAMPLE_COUNT; i++)
    {
        const float radius = max_coc * sqrt((float(i) + 0.5f) / DOF_SAMPLE_COUNT);
        const float angle = float(i) * DOF_GOLDEN_ANGLE;

        const float2 sample_uv = center_uv + float2(cos(angle), sin(angle)) * radius * texel_size;
        const float4 sample = half_res_texture.SampleLevel(SamplerPointClamp, sample_uv, 0);

        float sample_size = abs(sample.a);

        if (sample.a > center_coc)
            sample_size = min(sample_size, center_size * 2.0f);

        const float weight = saturate(sample_size - radius + 0.5f);

        color += float4(sample.rgb * weight, weight);

        if (sample.a < 0.0f)
            near_weight += weight;
    }

    const float near_coverage = saturate(near_weight * 2.0f / DOF_SAMPLE_COUNT);

    blur_texture[threadID] = float4(color.rgb / color.a, near_coverage);
}
