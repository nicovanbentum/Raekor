#include "Include/Bindless.hlsli"
#include "Include/DepthOfField.hlsli"

ROOT_CONSTANTS(DepthOfFieldRootConstants, rc)

[numthreads(8, 8, 1)]
void main(uint2 threadID : SV_DispatchThreadID)
{
    if (any(threadID >= rc.mDispatchSize))
        return;

    Texture2D<float4> input_texture = ResourceDescriptorHeap[rc.mInputTexture];
    Texture2D<float> depth_texture = ResourceDescriptorHeap[rc.mDepthTexture];
    RWTexture2D<float4> half_res_texture = ResourceDescriptorHeap[rc.mHalfResTexture];

    uint2 full_size;
    input_texture.GetDimensions(full_size.x, full_size.y);

    const float focus_distance = DoFGetFocusDistance(rc, depth_texture);

    float3 color = 0.xxx;
    float near_coc = 0.0f;
    float far_coc = 0.0f;

    for (uint i = 0; i < 4; i++)
    {
        uint2 texel = min(threadID * 2 + uint2(i & 1, i >> 1), full_size - 1);

        float coc = DoFGetCoC(DoFLinearizeDepth(depth_texture[texel], rc), focus_distance, rc);

        color += input_texture[texel].rgb;
        near_coc = min(near_coc, coc);
        far_coc = max(far_coc, coc);
    }

    float coc = near_coc < 0.0f ? near_coc : far_coc;

    half_res_texture[threadID] = float4(color * 0.25f, coc * 0.5f);
}
