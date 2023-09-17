#include "include/bindless.hlsli"
#include "include/common.hlsli"
#include "include/packing.hlsli"

ROOT_CONSTANTS(TAAResolveConstants, rc)


float4 main(in FULLSCREEN_TRIANGLE_VS_OUT inParams) : SV_Target0 {
    Texture2D color_texture = ResourceDescriptorHeap[rc.mColorTexture];
    Texture2D depth_texture = ResourceDescriptorHeap[rc.mDepthTexture];
    Texture2D history_texture = ResourceDescriptorHeap[rc.mHistoryTexture];
    Texture2D velocity_texture = ResourceDescriptorHeap[rc.mVelocityTexture];
    
    FrameConstants fc = gGetFrameConstants();
    
    float3 color_sample = color_texture[inParams.mPixelCoords.xy].rgb;
    
    if (fc.mFrameCounter == 0)
        return float4(color_sample, 1.0);
    
    float3 min_color = color_sample;
    float3 max_color = color_sample;
    
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            int2 pixel_pos = clamp(inParams.mPixelCoords.xy + int2(x, y), 0, rc.mRenderSize);
            float3 neighbor_sample = max(color_texture[pixel_pos].rgb, 0);
            
            min_color = min(min_color, neighbor_sample);
            max_color = max(max_color, neighbor_sample);
        }
    }
    
    float2 history_uv = velocity_texture[inParams.mPixelCoords.xy].xy;
    float3 history_sample = history_texture.Sample(SamplerLinearClamp, saturate(inParams.mScreenUV - history_uv)).rgb;
    
    history_sample = min(max_color, history_sample);
    history_sample = max(min_color, history_sample);
    
    return float4(lerp(history_sample, color_sample, 0.1), 1.0);
}