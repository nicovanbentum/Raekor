#include "Include/Bindless.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"
#include "Include/DDGI.hlsli"
#include "Include/Material.hlsli"
#include "Include/Sky.hlsli"
#include "Include/RayTracing.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(ProbeSampleRootConstants, rc)

float max3(float3 inValue)
{
    return max(max(inValue.x, inValue.y), inValue.z);
}

float min3(float3 inValue)
{
    return min(min(inValue.x, inValue.y), inValue.z);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    if (any(dispatchThreadID.xy >= rc.mDispatchSize.xy))
        return;
    
    Texture2D<float> depth_texture              = ResourceDescriptorHeap[rc.mDepthTexture];
    Texture2D<float4> gbuffer_texture           = ResourceDescriptorHeap[rc.mGBufferTexture];
    RWTexture2D<float> ddgi_depth_texture       = ResourceDescriptorHeap[rc.mDDGIData.mProbesDepthTexture];
    RWTexture2D<float3> ddgi_irradiance_texture = ResourceDescriptorHeap[rc.mDDGIData.mProbesIrradianceTexture];
    
    const float depth = depth_texture[dispatchThreadID.xy];
    RWTexture2D<float4> output_texture = ResourceDescriptorHeap[rc.mOutputTexture];
    
    if (depth >= 1.0f)
    {
        output_texture[dispatchThreadID.xy] = 0.xxxx;
        return;
    }
    
    float2 screen_uv = (float2(dispatchThreadID.xy) + 0.5f) / rc.mDispatchSize;
    float3 position_ws = ReconstructWorldPosition(screen_uv, depth, fc.mInvViewProjectionMatrix);
    float3 normal = UnpackNormal(asuint(gbuffer_texture[dispatchThreadID.xy]));
    
    float3 offset = min3(rc.mDDGIData.mProbeSpacing) * 0.1f;
    float3 offset_ws_pos = position_ws + normal * offset;
    float3 irradiance = DDGISampleIrradiance(offset_ws_pos, normal, rc.mDDGIData);
    
    output_texture[dispatchThreadID.xy] = float4(irradiance, 1.0);
}