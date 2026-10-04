#include "Include/Common.hlsli"
#include "Include/Bindless.hlsli"
#include "Include/Material.hlsli"
#include "Include/DDGI.hlsli"
#include "Include/Sky.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(LightingRootConstants, rc)

static Texture2D<float> ao_texture                  = ResourceDescriptorHeap[rc.mAmbientOcclusionTexture];
static Texture2D<float> depth_texture               = ResourceDescriptorHeap[rc.mGbufferDepthTexture];
static Texture2D<float2> shadow_texture             = ResourceDescriptorHeap[rc.mShadowMaskTexture];
static Texture2D<uint4> gbuffer_texture             = ResourceDescriptorHeap[rc.mGbufferRenderTexture];
static Texture2D<float4> reflections_texture        = ResourceDescriptorHeap[rc.mReflectionsTexture];
static Texture2D<float4> indirect_diffuse_texture   = ResourceDescriptorHeap[rc.mIndirectDiffuseTexture];
static TextureCube<float3> skycube_texture          = ResourceDescriptorHeap[rc.mSkyCubeTexture];
static TextureCube<float3> diffuse_cube_texture     = ResourceDescriptorHeap[rc.mDiffuseSkyCubeTexture];
static StructuredBuffer<RTLight> lights             = ResourceDescriptorHeap[fc.mLightsBuffer];
static Texture2D<float2> brdf_lut_texture           = ResourceDescriptorHeap[rc.mBrdfLutTexture];

float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0.xxx - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float3 ComputeIBL(Surface inSurface, float3 Wo)
{
    // TODO: improve prefiltering, currently just box filtering using SPD
    
    uint width, height, levels;
    skycube_texture.GetDimensions(0, width, height, levels);
    
    float lod = inSurface.mRoughness * (levels - 1);
    float3 dir = reflect(-Wo, inSurface.mNormal.rgb);
    float3 specular = skycube_texture.SampleLevel(SamplerLinearClamp, dir, lod);
    
    float cos_theta = saturate(dot(inSurface.mNormal, Wo));
    float3 F0 = lerp(0.04.xxx, inSurface.mAlbedo.rgb, inSurface.mMetallic);
    float3 F = fresnelSchlickRoughness(cos_theta, F0, inSurface.mRoughness);
    
    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - inSurface.mMetallic);
    
    float2 uv = float2(cos_theta, inSurface.mRoughness);
    float2 brdf = brdf_lut_texture.SampleLevel(SamplerLinearClamp, uv, 0.0).rg;
    
    float3 diffuse = diffuse_cube_texture.Sample(SamplerLinearClamp, inSurface.mNormal).rgb;
    float3 Ld = kD * diffuse * inSurface.mAlbedo.rgb;
    float3 Lr = (F * brdf.x + brdf.y) * specular;
    
    //return specular;
    
    return (kD * Ld) + Lr;
}

float4 main(in FULLSCREEN_TRIANGLE_VS_OUT inParams) : SV_Target0 
{
    Surface surface;
    surface.Unpack(asuint(gbuffer_texture[inParams.mPixelCoords.xy]));
    
    float depth = depth_texture[inParams.mPixelCoords.xy];
    const float3 ws_pos = ReconstructWorldPosition(inParams.mScreenUV, depth, fc.mInvViewProjectionMatrix);
    
    if (depth == 1.0) 
    {    
        float3 sky_color = skycube_texture.SampleLevel(SamplerLinearClamp, ws_pos, 0);
        return float4(max(sky_color, 0.0.xxx), 1.0);
    }

    // indirect diffuse and specular are attenuated by ambient occlusion
    float ao = ao_texture.SampleLevel(SamplerLinearClamp, inParams.mScreenUV, 0);
    
    const float3 Wo = normalize(fc.mCameraPosition.xyz - ws_pos.xyz);

    float3 total_radiance = surface.mEmissive;

    // evaluate DirectionalLight 
    if (1) 
    {
        float3 Wi = normalize(-fc.mSunDirection.xyz);
        float sun_shadow = shadow_texture.SampleLevel(SamplerLinearClamp, inParams.mScreenUV, 0).r;
        total_radiance += EvaluateDirectionalLight(surface, fc.mSunColor, Wi, Wo) * sun_shadow * ao;
    }

    uint2 group_index = uint2(inParams.mPixelCoords.xy) / LIGHT_CULL_TILE_SIZE;
    RWByteAddressBuffer light_count_buffer = ResourceDescriptorHeap[rc.mLights.mLightGridBuffer];
    RWByteAddressBuffer light_index_buffer = ResourceDescriptorHeap[rc.mLights.mLightIndicesBuffer];
        
    uint light_count = light_count_buffer.Load(rc.mLights.mDispatchSize.x * group_index.x + group_index.y);
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
                total_radiance += EvaluatePointLight(surface, light, Wi, Wo, dist_to_light) * ao;
            } break;

            case RT_LIGHT_TYPE_SPOT:
            {
                float3 Wi = SampleSpotLight(light, ws_pos);
                total_radiance += EvaluateSpotLight(surface, light, Wi, Wo, dist_to_light) * ao;
            } break;
        }
    }


#if 0
    total_radiance += ComputeIBL(surface, Wo) * ao;
    
    //float3 indirect_diffuse = diffuse_cube_texture.Sample(SamplerLinearClamp, surface.mNormal);
    //total_radiance += indirect_diffuse.rgb * (1.0 - kS) * surface.mAlbedo.rgb * ao;
#else
    // evaluate indirect specular
    float4 specular = reflections_texture.SampleLevel(SamplerLinearClamp, inParams.mScreenUV, 0);
    total_radiance += specular.rgb * surface.mAlbedo.rgb * ao;
    
    // evaluate indirect diffuse
    float4 diffuse = indirect_diffuse_texture.SampleLevel(SamplerLinearClamp, inParams.mScreenUV, 0);
    total_radiance += diffuse.rgb * surface.mAlbedo.rgb * ao;
#endif
    
    return float4(total_radiance * fc.mExposure, 1.0);
}