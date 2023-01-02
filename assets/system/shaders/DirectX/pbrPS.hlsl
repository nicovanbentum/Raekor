#include "include/brdf.hlsli"
#include "include/shared.h"
#include "include/common.hlsli"

struct VS_OUTPUT {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

SRV0(FrameConstantsBuffer)
SRV1(PassConstantsBuffer)
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

float4 main(in VS_OUTPUT input) : SV_Target0 {
    Texture2D<float>    depth_texture   = ResourceDescriptorHeap[rc.mGbufferDepthTexture];
    Texture2D<float4>   shadow_texture  = ResourceDescriptorHeap[rc.mShadowMaskTexture];
    Texture2D<uint4>    gbuffer_texture = ResourceDescriptorHeap[rc.mGbufferRenderTexture];

    float shadow_mask = shadow_texture[input.pos.xy].x;

    BRDF brdf;
    brdf.Unpack(asuint(gbuffer_texture[input.pos.xy]));
    
    float3 total_radiance = float3(0.0, 0.0, 0.0);

    FrameConstants fc = FrameConstantsBuffer.Load<FrameConstants>(0);
    const float3 position = ReconstructPosFromDepth(input.uv, depth_texture[input.pos.xy], fc.mInvViewProjectionMatrix);

    const float3 Wo = normalize(fc.mCameraPosition.xyz - position.xyz);
    const float3 Wi = normalize(-fc.mSunDirection.xyz);
    const float3 Wh = normalize(Wo + Wi);
    const float3 l = brdf.Evaluate(Wo, Wi, Wh);

    const float NdotL = max(dot(brdf.mNormal, Wi), 0.0);
    const float3 light_color = float3(1.0, 1.0, 1.0); // TODO: sky calculations
    total_radiance += l * NdotL * (light_color * shadow_mask);

    //total_radiance += brdf.mAlbedo.rgb * 0.22;

    total_radiance = pow(brdf.mAlbedo.rgb, 1.0 / 2.2);
    
    return float4(brdf.mAlbedo.rgb, 1.0);
}