#include "include/bindless.hlsli"
#include "include/common.hlsli"
#include "include/packing.hlsli"
#include "include/brdf.hlsli"

ROOT_CONSTANTS(GbufferDebugRootConstants, rc)

float4 main(in FULLSCREEN_TRIANGLE_VS_OUT inParams) : SV_Target0 {
    Texture2D gbuffer_texture = ResourceDescriptorHeap[rc.mTexture];
    float4 output_color = gbuffer_texture[int2(inParams.mPixelCoords.xy)];
    
#ifdef DEBUG_TEXTURE_GBUFFER_DEPTH
    float depth = gbuffer_texture.Sample(SamplerPointClamp, inParams.mScreenUV).r;
    float linear_depth = rc.mNearPlane * rc.mFarPlane / (rc.mFarPlane + depth * (rc.mNearPlane - rc.mFarPlane));
    output_color = float4(1.0f / linear_depth.x, 0.0, 0.0, 1.0);
#endif
    
#ifdef DEBUG_TEXTURE_GBUFFER_VELOCITY
    output_color = float4(output_color.rg * 100.0f, 0.0, 1.0);
#endif
   
    BRDF brdf;
    brdf.Unpack(asuint(output_color));
    
#if defined(DEBUG_TEXTURE_GBUFFER_ALBEDO)
    output_color = brdf.mAlbedo;

#elif defined(DEBUG_TEXTURE_GBUFFER_NORMALS)
    output_color.rgb = brdf.mNormal * 0.5 + 0.5;
    
#elif defined(DEBUG_TEXTURE_GBUFFER_EMISSIVE)
    output_color.rgb = brdf.mEmissive;

#elif defined(DEBUG_TEXTURE_GBUFFER_METALLIC)
    output_color = brdf.mMetallic.xxxx;

#elif defined(DEBUG_TEXTURE_GBUFFER_ROUGHNESS)
    output_color = brdf.mRoughness.xxxx;
    
#endif
    
    return float4(output_color.rgb, 1.0);
}