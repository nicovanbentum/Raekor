#include "Include/Shared.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"
#include "Include/Bindless.hlsli"


ROOT_CONSTANTS(SSRTraceRootConstants, rc)

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    if (any(threadID.xy >= rc.mDispatchSize.xy))
        return;
    
    Texture2D<float>  depth_texture    = ResourceDescriptorHeap[rc.mDepthTexture];
    Texture2D<float4> scene_texture    = ResourceDescriptorHeap[rc.mSceneTexture];
    Texture2D<float4> gbuffer_texture  = ResourceDescriptorHeap[rc.mGBufferTexture];
    RWTexture2D<float4> output_texture = ResourceDescriptorHeap[rc.mOutputTexture];
    
    FrameConstants fc = gGetFrameConstants();
    
    float2 pixel_center = float2(threadID.xy) + float2(0.5, 0.5);
    float2 screen_uv = pixel_center / rc.mDispatchSize;
    
    float depth = depth_texture[threadID.xy];
    
    if (depth == 1.0)
    {
        output_texture[threadID.xy] = 1.0;
        return;
    }
    
    float3 ws_pos = ReconstructWorldPosition(screen_uv, depth, fc.mInvViewProjectionMatrix);
    float3 ws_normal = UnpackNormal(asuint(gbuffer_texture[threadID.xy]));
    
    float3 camera_dir = normalize(fc.mCameraPosition.xyz - ws_pos);
    float3 ws_ray_dir = normalize(reflect(camera_dir, ws_normal));
    
    float4 cs_ray_start = mul(fc.mViewProjectionMatrix, float4(ws_pos, 1.0));
    float4 cs_ray_end = mul(fc.mViewProjectionMatrix, float4(ws_pos + ws_ray_dir, 1.0));
    
    float3 ndc_ray_start = cs_ray_start.xyz / cs_ray_start.w;
    float3 ndc_ray_end = cs_ray_end.xyz / cs_ray_end.w;
    
    ndc_ray_start.xy = ndc_ray_start.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
    ndc_ray_end.xy = ndc_ray_end.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
    
    float3 ndc_ray_dir = normalize(ndc_ray_end - ndc_ray_start);
    
    float sample_depth = 0.0;
    float3 sample_color = 0.0.xxx;
    
    for (int i = 1; i < 64; i++)
    {
        float3 ndc_sample_pos = ndc_ray_start + ndc_ray_dir * i;
        
        if (any(ndc_sample_pos.xy < 0.0) || any(ndc_sample_pos.xy > 1.0))
        {
            sample_color = 0.xxx;
            break;
        }
        
        sample_depth = depth_texture.SampleLevel(SamplerPointClamp, ndc_sample_pos.xy, 0);
        
        if (sample_depth > depth)
        {
            sample_color = scene_texture.SampleLevel(SamplerLinearClamp, ndc_sample_pos.xy, 0);
            break;
        }
        
        ndc_ray_dir *= 1.05;
        
        //sample_color = scene_texture.SampleLevel(SamplerLinearClamp, ndc_sample_pos.xy, 0);
        
    }
        
    output_texture[threadID.xy] = float4(sample_color, 1.0);
}