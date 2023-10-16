#include "include/bindless.hlsli"
#include "include/packing.hlsli"

struct VS_OUTPUT {
    float4 sv_position : SV_Position;
    float4 curr_position : POS0;
    float4 prev_position : POS1;
    float2 texcoord : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BINORMAL;
};

struct PS_OUTPUT {
    float4 gbuffer: SV_Target0;
    float2 motionvectors : SV_Target1;
};

ROOT_CONSTANTS(GbufferRootConstants, rc)


PS_OUTPUT main(in VS_OUTPUT input) {
    PS_OUTPUT output;

    Texture2D<float4> albedo_texture = ResourceDescriptorHeap[rc.mAlbedoTexture];
    Texture2D<float4> normal_texture = ResourceDescriptorHeap[rc.mNormalTexture];
    Texture2D<float4> emissive_texture = ResourceDescriptorHeap[rc.mEmissiveTexture];
    Texture2D<float4> metallic_texture = ResourceDescriptorHeap[rc.mMetallicTexture];
    Texture2D<float4> roughness_texture = ResourceDescriptorHeap[rc.mRoughnessTexture];
    
    float4 sampled_albedo = albedo_texture.Sample(SamplerAnisoWrap, input.texcoord);
    if (sampled_albedo.a < 0.3)
        discard;
    
    float4 sampled_normal = normal_texture.Sample(SamplerAnisoWrap, input.texcoord);
    float sampled_metallic = metallic_texture.Sample(SamplerAnisoWrap, input.texcoord).r;
    float sampled_roughness = roughness_texture.Sample(SamplerAnisoWrap, input.texcoord).r;
    
    FrameConstants fc = gGetFrameConstants();

    float3x3 TBN = transpose(float3x3(input.tangent, input.bitangent, input.normal));
    float3 normal = normalize(mul(TBN, sampled_normal.xyz * 2.0 - 1.0));
    // normal = normalize(input.normal);

    float4 albedo = rc.mAlbedo * sampled_albedo;
    float metalness = rc.mMetallic * sampled_metallic;
    float roughness = rc.mRoughness * sampled_roughness;
    
    uint4 packed = uint4(0, 0, 0, 0);
    PackAlbedo(albedo, packed);
    PackNormal(normal, packed);
    PackMetallicRoughness(metalness, roughness, packed);
    
    output.gbuffer = asfloat(packed);
    
    float2 curr_pos = (input.curr_position.xyz / input.prev_position.w).xy - fc.mJitter;
    float2 prev_pos = (input.prev_position.xyz / input.prev_position.w).xy - fc.mPrevJitter;
    
    output.motionvectors = (curr_pos - prev_pos);
    output.motionvectors.xy *= float2(0.5, -0.5);
    
    return output;
}