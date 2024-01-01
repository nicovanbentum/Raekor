#include "include/bindless.hlsli"
#include "include/packing.hlsli"
#include "include/common.hlsli"
#include "include/random.hlsli"
#include "include/ddgi.hlsli"
#include "include/brdf.hlsli"
#include "include/sky.hlsli"
#include "include/rt.hlsli"

ROOT_CONSTANTS(ProbeSampleRootConstants, rc)

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    if (any(dispatchThreadID.xy >= rc.mDispatchSize.xy))
        return;
    
    Texture2D<float> depth_texture              = ResourceDescriptorHeap[rc.mDepthTexture];
    Texture2D<float4> gbuffer_texture           = ResourceDescriptorHeap[rc.mGBufferTexture];
    RWTexture2D<float> ddgi_depth_texture       = ResourceDescriptorHeap[rc.mDDGIData.mProbesDepthTexture];
    RWTexture2D<float3> ddgi_irradiance_texture = ResourceDescriptorHeap[rc.mDDGIData.mProbesIrradianceTexture];
    
    FrameConstants fc = gGetFrameConstants();
    
    const float depth = depth_texture[dispatchThreadID.xy];
    RWTexture2D<float4> output_texture = ResourceDescriptorHeap[rc.mOutputTexture];
    
    if (depth >= 1.0f)
    {
        output_texture[dispatchThreadID.xy] = 0.xxxx;
        return;
    }
    
    const float3 normal = UnpackNormal(asuint(gbuffer_texture[dispatchThreadID.xy]));
    
    const float2 screen_uv = (float2(dispatchThreadID.xy) + 0.5f) / rc.mDispatchSize;
    const float3 ws_pos = ReconstructWorldPosition(screen_uv, depth, fc.mInvViewProjectionMatrix);
    
    float3 offset_ws_pos = ws_pos + normal * 0.01;
    float3 irradiance = DDGISampleIrradiance(offset_ws_pos, normal, rc.mDDGIData);
    
    output_texture[dispatchThreadID.xy] = float4(irradiance, 1.0);
}