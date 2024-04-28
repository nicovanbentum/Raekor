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
    uint selection : SV_Target2;
};

ROOT_CONSTANTS(GbufferRootConstants, rc)


float3 ReconstructNormalBC5(float2 normal)
{
    float2 xy = 2.0f * normal - 1.0f;
    float z = sqrt(1 - dot(xy, xy));
    return float3(xy.x, xy.y, z);
}

PS_OUTPUT main(in VS_OUTPUT input) {
    PS_OUTPUT output;

    StructuredBuffer<RTGeometry> geometries = ResourceDescriptorHeap[rc.mInstancesBuffer];
    StructuredBuffer<RTMaterial> materials = ResourceDescriptorHeap[rc.mMaterialsBuffer];
    
    RTGeometry geometry = geometries[rc.mInstanceIndex];
    RTMaterial material = materials[geometry.mMaterialIndex];
    
    Texture2D albedo_texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.mAlbedoTexture)];
    Texture2D normals_texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.mNormalsTexture)];
    Texture2D emissive_texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.mEmissiveTexture)];
    Texture2D metallic_texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.mMetallicTexture)];
    Texture2D roughness_texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.mRoughnessTexture)];
    
    float4 sampled_albedo = albedo_texture.Sample(SamplerAnisoWrap, input.texcoord);
    if (sampled_albedo.a < 0.3)
        discard;
    
    float3 sampled_normal = normals_texture.Sample(SamplerAnisoWrap, input.texcoord).rgb; // alpha channel unused
    float3 sampled_emissive = emissive_texture.Sample(SamplerAnisoWrap, input.texcoord).rgb; // alpha channel unused
    float sampled_metallic = metallic_texture.Sample(SamplerAnisoWrap, input.texcoord).r; // value swizzled across all channels, just get Red
    float sampled_roughness = roughness_texture.Sample(SamplerAnisoWrap, input.texcoord).r; // value swizzled across all channels, just get Red
        
    //sampled_normal = sampled_normal * 2.0 - 1.0;
    sampled_normal = ReconstructNormalBC5(sampled_normal.xy);
    
    FrameConstants fc = gGetFrameConstants();

    float3x3 TBN = transpose(float3x3(input.tangent, input.bitangent, input.normal));
    float3 normal = normalize(mul(TBN, sampled_normal.xyz));
    // normal = normalize(input.normal);

    float4 albedo = material.mAlbedo * sampled_albedo;
    float metalness = material.mMetallic * sampled_metallic;
    float roughness = material.mRoughness * sampled_roughness;
    float3 emissive = material.mEmissive.rgb * sampled_emissive;
    
    uint4 packed = uint4(0, 0, 0, 0);
    PackAlbedo(albedo.rgb, packed);
    PackAlpha(albedo.a, packed);
    PackNormal(normal, packed);
    PackEmissive(emissive, packed);
    PackMetallicRoughness(metalness, roughness, packed);

    output.gbuffer = asfloat(packed);
    
    float2 curr_pos = (input.curr_position.xyz / input.prev_position.w).xy - fc.mJitter;
    float2 prev_pos = (input.prev_position.xyz / input.prev_position.w).xy - fc.mPrevJitter;
    
    output.motionvectors = (curr_pos - prev_pos);
    output.motionvectors.xy *= float2(0.5, -0.5);
    
    output.selection = rc.mEntity;
    
    return output;
}