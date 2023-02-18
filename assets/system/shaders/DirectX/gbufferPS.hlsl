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

struct RootConstants {
    float4 albedo;
    uint4 textures;
    float4 properties;
};

ROOT_CONSTANTS(RootConstants, root_constants)


//[RootSignature(GLOBAL_ROOT_SIGNATURE)]
PS_OUTPUT main(in VS_OUTPUT input) {
    PS_OUTPUT output;

    Texture2D<float4> albedo_texture = ResourceDescriptorHeap[root_constants.textures.x];
    Texture2D<float4> normal_texture = ResourceDescriptorHeap[root_constants.textures.y];
    Texture2D<float4> material_texture = ResourceDescriptorHeap[root_constants.textures.z];
    
    float4 sampled_albedo = albedo_texture.Sample(SamplerAnisoWrap, input.texcoord);
    float4 sampled_normal = normal_texture.Sample(SamplerAnisoWrap, input.texcoord);
    float4 sampled_material = material_texture.Sample(SamplerAnisoWrap, input.texcoord);

    float3x3 TBN = transpose(float3x3(input.tangent, input.bitangent, input.normal));
    float3 normal = normalize(mul(TBN, sampled_normal.xyz * 2.0 - 1.0));
    // normal = normalize(input.normal);

    float4 albedo = root_constants.albedo * sampled_albedo;
    float metalness = root_constants.properties.x * sampled_material.b;
    float roughness = root_constants.properties.y * sampled_material.g;

    uint4 packed = uint4(0, 0, 0, 0);
    PackAlbedo(albedo, packed);
    PackNormal(normal, packed);
    PackMetallicRoughness(metalness, roughness, packed);
    
    float2 prev_pos = (input.prev_position.xyz / input.prev_position.w).xy;
    float2 curr_pos = (input.curr_position.xyz / input.prev_position.w).xy;
    
    output.gbuffer = asfloat(packed);
    output.motionvectors = (input.prev_position.xy / input.prev_position.w) - (input.curr_position.xy / input.curr_position.w);
    output.motionvectors *= float2(0.5, -0.5);
    
    return output;
}