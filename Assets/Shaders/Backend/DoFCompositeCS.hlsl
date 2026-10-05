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
    Texture2D<float4> blur_texture = ResourceDescriptorHeap[rc.mBlurTexture];
    RWTexture2D<float4> output_texture = ResourceDescriptorHeap[rc.mOutputTexture];

    const float focus_distance = DoFGetFocusDistance(rc, depth_texture);
    const float coc = DoFGetCoC(DoFLinearizeDepth(depth_texture[threadID], rc), focus_distance, rc);

    const float2 uv = (float2(threadID) + 0.5f) / float2(rc.mDispatchSize);
    const float4 blur = blur_texture.SampleLevel(SamplerLinearClamp, uv, 0);

    const float4 color = input_texture[threadID];
    const float blend = max(saturate(abs(coc) * 0.5f - 0.5f), blur.a);

    output_texture[threadID] = float4(lerp(color.rgb, blur.rgb, blend), color.a);
}
