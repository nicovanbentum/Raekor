#include "Include/Common.hlsli"
#include "Include/Bindless.hlsli"
#include "Include/Material.hlsli"
#include "Include/DDGI.hlsli"
#include "Include/Sky.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(LightingRootConstants, rc)

float4 main(in FULLSCREEN_TRIANGLE_VS_OUT inParams) : SV_Target0 
{
    Texture2D<float>    ao_texture                = ResourceDescriptorHeap[rc.mAmbientOcclusionTexture];
    Texture2D<float>    depth_texture             = ResourceDescriptorHeap[rc.mGbufferDepthTexture];
    Texture2D<float2>   shadow_texture            = ResourceDescriptorHeap[rc.mShadowMaskTexture];
    Texture2D<uint4>    gbuffer_texture           = ResourceDescriptorHeap[rc.mGbufferRenderTexture];
    Texture2D<float4>   reflections_texture       = ResourceDescriptorHeap[rc.mReflectionsTexture];
    Texture2D<float4>   indirect_diffuse_texture  = ResourceDescriptorHeap[rc.mIndirectDiffuseTexture];
    // Texture2D<float4>   diffuse_gi_texture        = ResourceDescriptorHeap[rc.mIndirectDiffuseTexture];
    TextureCube<float3> skycube_texture           = ResourceDescriptorHeap[rc.mSkyCubeTexture];
    StructuredBuffer<RTLight> lights              = ResourceDescriptorHeap[fc.mLightsBuffer];

    Surface surface;
    surface.Unpack(asuint(gbuffer_texture[inParams.mPixelCoords.xy]));
    
    float depth = depth_texture[inParams.mPixelCoords.xy];
    const float3 ws_pos = ReconstructWorldPosition(inParams.mScreenUV, depth, fc.mInvViewProjectionMatrix);
    
    if (depth == 1.0) 
    {    
        float3 sky_color = skycube_texture.SampleLevel(SamplerLinearClamp, ws_pos, 0);
        return float4(max(sky_color, 0.0.xxx), 1.0);
    }

    const float3 Wo = normalize(fc.mCameraPosition.xyz - ws_pos.xyz);

    float3 total_radiance = surface.mEmissive;

    // evaluate DirectionalLight 
    {
        float3 Wi = normalize(-fc.mSunDirection.xyz);
        float sun_shadow = shadow_texture.SampleLevel(SamplerLinearClamp, inParams.mScreenUV, 0).r;
        total_radiance += EvaluateDirectionalLight(surface, fc.mSunColor, Wi, Wo) * sun_shadow;
    }

    uint2 group_index = uint2(inParams.mPixelCoords.xy) / LIGHT_CULL_TILE_SIZE;
    RWByteAddressBuffer light_count_buffer = ResourceDescriptorHeap[rc.mLights.mLightGridBuffer];
    RWByteAddressBuffer light_index_buffer = ResourceDescriptorHeap[rc.mLights.mLightIndicesBuffer];
        
    uint light_count = light_count_buffer.Load(rc.mLights.mDispatchSize.x.x * group_index.x + group_index.y);
    uint index_offset = rc.mLights.mDispatchSize.x * LIGHT_CULL_MAX_LIGHTS * group_index.y + group_index.x * LIGHT_CULL_MAX_LIGHTS;
    
    // evaluate Point and Spot lights
    for (int light_idx = 0; light_idx < fc.mNrOfLights; light_idx++)
    //for (uint light_idx = 0; light_idx < light_count; light_idx++)
    {
        RTLight light = lights[light_idx];
        //RTLight light = lights[light_index_buffer.Load(index_offset + light_idx)];
        float dist_to_light = length(light.mPosition.xyz - ws_pos);

        switch (light.mType)
        {
            case RT_LIGHT_TYPE_POINT:
            {
                float3 Wi = SamplePointLight(light, ws_pos);
                total_radiance += EvaluatePointLight(surface, light, Wi, Wo, dist_to_light);
            } break;

            case RT_LIGHT_TYPE_SPOT:
            {
                float3 Wi = SampleSpotLight(light, ws_pos);
                total_radiance += EvaluateSpotLight(surface, light, Wi, Wo, dist_to_light);
            } break;
        }
    }

    // indirect diffuse and specular are attenuated by RTAO
    float ao = ao_texture.SampleLevel(SamplerLinearClamp, inParams.mScreenUV, 0);
    
    // evaluate indirect specular
    float4 specular = reflections_texture.SampleLevel(SamplerLinearClamp, inParams.mScreenUV, 0);
    total_radiance += specular.rgb * surface.mAlbedo.rgb * ao;
    
    // evaluate indirect diffuse
    float3 irradiance = indirect_diffuse_texture.SampleLevel(SamplerLinearClamp, inParams.mScreenUV, 0).rgb;
    total_radiance += irradiance.rgb * surface.mAlbedo.rgb * ao;
    
    return float4(total_radiance, 1.0);
}