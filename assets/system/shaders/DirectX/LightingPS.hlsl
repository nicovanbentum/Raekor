#include "include/bindless.hlsli"
#include "include/common.hlsli"
#include "include/brdf.hlsli"
#include "include/ddgi.hlsli"
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
    Texture2D<float>    ao_texture                = ResourceDescriptorHeap[rc.mAmbientOcclusionTexture];
    Texture2D<float>    depth_texture             = ResourceDescriptorHeap[rc.mGbufferDepthTexture];
    Texture2D<float2>   shadow_texture            = ResourceDescriptorHeap[rc.mShadowMaskTexture];
    Texture2D<uint4>    gbuffer_texture           = ResourceDescriptorHeap[rc.mGbufferRenderTexture];
    Texture2D<float4>   reflections_texture       = ResourceDescriptorHeap[rc.mReflectionsTexture];
    Texture2D<float4>   indirect_diffuse_texture  = ResourceDescriptorHeap[rc.mIndirectDiffuseTexture];
    // Texture2D<float4>   diffuse_gi_texture        = ResourceDescriptorHeap[rc.mIndirectDiffuseTexture];

    BRDF brdf;
    brdf.Unpack(asuint(gbuffer_texture[inParams.mPixelCoords.xy]));
    
    float3 total_radiance = float3(0.0, 0.0, 0.0);

    float depth = depth_texture[inParams.mPixelCoords.xy];
    FrameConstants fc = gGetFrameConstants();
    const float3 ws_pos = ReconstructWorldPosition(inParams.mScreenUV, depth, fc.mInvViewProjectionMatrix);
    
    if (depth == 1.0) {
        float3 transmittance;
        const float3 light_color = fc.mSunColor.rgb;
        float3 inscattering = IntegrateScattering(ws_pos, normalize(fc.mCameraPosition.xyz - ws_pos), 1.#INF, fc.mSunDirection.xyz, fc.mSunColor.rgb, transmittance);
        //sky_color = ApplyFog(sky_color, distance(fc.mCameraPosition.xyz, position.xyz), fc.mCameraPosition.xyz, normalize(fc.mCameraPosition.xyz - position.xyz));
        return float4(max(inscattering, 0.0.xxx) * fc.mSunColor.a, 1.0);
    }

    const float3 Wo = normalize(fc.mCameraPosition.xyz - ws_pos.xyz);
    const float3 Wi = normalize(-fc.mSunDirection.xyz);
    const float3 Wh = normalize(Wo + Wi);
    const float3 l = brdf.Evaluate(Wo, Wi, Wh);

    const float NdotL = max(dot(brdf.mNormal, Wi), 0.0);
    float3 sunlight_luminance = Absorb(IntegrateOpticalDepth(0.xxx, fc.mSunDirection.xyz)) * fc.mSunColor.a;
    float shadow_mask   = shadow_texture[inParams.mPixelCoords.xy].x;
    total_radiance += l * NdotL * sunlight_luminance * shadow_mask;
    
    float ao = ao_texture[inParams.mPixelCoords.xy];
    //ao = 1.0;
    
    float4 specular = reflections_texture.SampleLevel(SamplerLinearClamp, inParams.mScreenUV, 0);
    total_radiance += specular.rgb * brdf.mAlbedo.rgb * ao;
    
    float3 offset_ws_pos = ws_pos + brdf.mNormal * 0.01;
    float3 irradiance = indirect_diffuse_texture[inParams.mPixelCoords.xy].rgb;
    
    // total_radiance += brdf.mAlbedo.rgb * 0.25;
    
    total_radiance += irradiance.rgb * brdf.mAlbedo.rgb * ao;
    
    //total_radiance = ApplyFog(total_radiance, distance(fc.mCameraPosition.xyz, ws_pos), fc.mCameraPosition.xyz, -Wo);
    
    //return float4(ao, ao, ao, 1.0);
    // return float4(irradiance.rgb, 1.0);
    return float4(total_radiance, 1.0);
}