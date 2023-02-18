#include "include/bindless.hlsli"
#include "include/common.hlsli"
#include "include/brdf.hlsli"
#include "include/sky.hlsli"

ROOT_CONSTANTS(LightingRootConstants, rc)

float3 ApplyFog(in float3 rgb, // original color of the pixel
               in float distance, // camera to point distance
               in float3 rayOri, // camera position
               in float3 rayDir)  // camera to point vector
{
    const float a = 0.001;
    const float absorption_coefficient = 0.02;
    float fogAmount = (a / absorption_coefficient) * exp(rayOri.y * absorption_coefficient) * (1.0 - exp(distance * rayDir.y * absorption_coefficient)) / -rayDir.y;
    fogAmount = saturate(fogAmount);
    float3 fogColor = float3(0.8, 0.6, 0.7);
    return lerp(rgb, fogColor, fogAmount);
}

float4 main(in FULLSCREEN_TRIANGLE_VS_OUT inParams) : SV_Target0 {
    Texture2D<float>    ao_texture      = ResourceDescriptorHeap[rc.mAmbientOcclusionTexture];
    Texture2D<float>    depth_texture   = ResourceDescriptorHeap[rc.mGbufferDepthTexture];
    Texture2D<float4>   shadow_texture  = ResourceDescriptorHeap[rc.mShadowMaskTexture];
    Texture2D<uint4>    gbuffer_texture = ResourceDescriptorHeap[rc.mGbufferRenderTexture];
    Texture2D<float4>   reflections_texture = ResourceDescriptorHeap[rc.mReflectionsTexture];
    

    BRDF brdf;
    brdf.Unpack(asuint(gbuffer_texture[inParams.mPixelCoords.xy]));
    
    float3 total_radiance = float3(0.0, 0.0, 0.0);

    float depth = depth_texture[inParams.mPixelCoords.xy];
    FrameConstants fc = gGetFrameConstants();
    const float3 position = ReconstructWorldPosition(inParams.mScreenUV, depth, fc.mInvViewProjectionMatrix);
    
    const float3 light_color = float3(1.0, 1.0, 1.0); // TODO: sky calculations
    
    if (depth == 1.0) {
        float3 transmittance;
        float3 sky_color = IntegrateScattering(position, normalize(fc.mCameraPosition.xyz - position), 1.#INF, fc.mSunDirection.xyz, light_color, transmittance);
        //sky_color = ApplyFog(sky_color, distance(fc.mCameraPosition.xyz, position.xyz), fc.mCameraPosition.xyz, normalize(fc.mCameraPosition.xyz - position.xyz));
        return float4(pow(sky_color, 1.0 / 2.2), 1.0);
    }

    const float3 Wo = normalize(fc.mCameraPosition.xyz - position.xyz);
    const float3 Wi = normalize(-fc.mSunDirection.xyz);
    const float3 Wh = normalize(Wo + Wi);
    const float3 l = brdf.Evaluate(Wo, Wi, Wh);

    const float NdotL = max(dot(brdf.mNormal, Wi), 0.0);
    float3 sunlight_luminance = Absorb(IntegrateOpticalDepth(0.xxx, -Wi));
    float shadow_mask   = shadow_texture[inParams.mPixelCoords.xy].x;
    total_radiance += l * NdotL * shadow_mask;
    
    if (brdf.mRoughness < 0.3)
    {
        float4 specular = reflections_texture.SampleLevel(SamplerLinearClamp, inParams.mScreenUV, lerp(0, 5, brdf.mRoughness));
        total_radiance += specular.rgb * shadow_mask;
    }
    
    
    float ao = ao_texture[inParams.mPixelCoords.xy];
    
    // total_radiance = ApplyFog(total_radiance, distance(fc.mCameraPosition.xyz, position.xyz), fc.mCameraPosition.xyz, -Wo);
    
    return float4(ao.xxx, 1.0);
}