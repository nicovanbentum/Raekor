#include "include/bindless.hlsli"
#include "include/brdf.hlsli"
#include "include/sky.hlsli"

struct VS_OUTPUT {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

ROOT_CONSTANTS(LightingRootConstants, rc)

float map(float value, float start1, float stop1, float start2, float stop2) {
    return start2 + (stop2 - start2) * ((value - start1) / (stop1 - start1));
}

float3 ReconstructPosFromDepth(float2 uv, float depth, float4x4 InvVP) {
    float x = uv.x * 2.0f - 1.0f;
    float y = (1.0 - uv.y) * 2.0f - 1.0f;
    float4 position_s = float4(x, y, depth, 1.0f);
    float4 position_v = mul(InvVP, position_s);
    return position_v.xyz / position_v.w;
}

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

float4 main(in VS_OUTPUT input) : SV_Target0 {
    Texture2D<float>    depth_texture   = ResourceDescriptorHeap[rc.mGbufferDepthTexture];
    Texture2D<float4>   shadow_texture  = ResourceDescriptorHeap[rc.mShadowMaskTexture];
    Texture2D<uint4>    gbuffer_texture = ResourceDescriptorHeap[rc.mGbufferRenderTexture];
    
    float shadow_mask = shadow_texture[input.pos.xy].x;

    BRDF brdf;
    brdf.Unpack(asuint(gbuffer_texture[input.pos.xy]));
    
    float3 total_radiance = float3(0.0, 0.0, 0.0);

    float depth = depth_texture[input.pos.xy];
    FrameConstants fc = gGetFrameConstants();
    const float3 position = ReconstructPosFromDepth(input.uv, depth, fc.mInvViewProjectionMatrix);
    
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
    total_radiance += l * NdotL * shadow_mask;
    
    // total_radiance = ApplyFog(total_radiance, distance(fc.mCameraPosition.xyz, position.xyz), fc.mCameraPosition.xyz, -Wo);
    
    //total_radiance += brdf.mAlbedo.rgb * 0.22;
    
    return float4(total_radiance, 1.0);
}