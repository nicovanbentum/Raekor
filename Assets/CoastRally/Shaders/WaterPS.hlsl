#include "Include/Bindless.hlsli"
#include "Include/Material.hlsli"
#include "Include/Packing.hlsli"

struct VS_OUTPUT {
    float4 sv_position : SV_Position;
    float4 curr_position : POS0;
    float4 prev_position : POS1;
    float2 texcoord : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BINORMAL;
    nointerpolation uint instance_index : INSTANCE_INDEX;
};

struct PS_OUTPUT {
    float4 gbuffer: SV_Target0;
    float2 motionvectors : SV_Target1;
    uint selection : SV_Target2;
};

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(GbufferRootConstants, rc)

void Procedure0(float3 inUV, float inTime, float3 inNormal, out float3 outNormal)
{
 const float4 ripples[6] =
{
    float4(0.94, 0.34, 3.6, 0.050),
    float4(-0.52, 0.85, 2.7, 0.040),
    float4(0.17, -0.98, 2.1, 0.030),
    float4(-0.88, -0.47, 1.6, 0.022),
    float4(0.70, -0.71, 1.25, 0.016),
    float4(-0.29, 0.96, 0.95, 0.012)
};

float2 slope = float2(0.0, 0.0);

[unroll]
for (int index = 0; index < 6; index++)
{
    const float2 direction = normalize(ripples[index].xy);
    const float k = 6.2831853 / ripples[index].z;
    const float speed = sqrt(9.81 / k);
    const float phase = k * (dot(direction, inUV.xy) - speed * inTime) + index * 1.7;
    slope += direction * (k * ripples[index].w * cos(phase));
}

outNormal = normalize(inNormal - float3(slope.x, 0.0, slope.y)); 
}

void Procedure4(float3 inUV, out float outNoise)
{
 outNoise = GradientNoise(inUV.xy); 
}



PS_OUTPUT main(in VS_OUTPUT input) {
    PS_OUTPUT output;

    StructuredBuffer<RTGeometry> geometries = ResourceDescriptorHeap[fc.mInstancesBuffer];
    StructuredBuffer<RTMaterial> materials = ResourceDescriptorHeap[fc.mMaterialsBuffer];

    RTGeometry geometry = geometries[input.instance_index];
    RTMaterial material = materials[geometry.mMaterialIndex];

    Texture2D albedo_texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.mAlbedoTexture)];
    Texture2D normals_texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.mNormalsTexture)];
    Texture2D emissive_texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.mEmissiveTexture)];
    Texture2D metallic_texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.mMetallicTexture)];
    Texture2D roughness_texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.mRoughnessTexture)];
    SamplerState material_sampler = GetMaterialSampler(material);

    float4 sampled_albedo = albedo_texture.Sample(material_sampler, input.texcoord);
    float3 sampled_normal = normals_texture.Sample(material_sampler, input.texcoord).rgb;
    float3 sampled_emissive = emissive_texture.Sample(material_sampler, input.texcoord).rgb;
    float sampled_metallic = metallic_texture.Sample(material_sampler, input.texcoord).r;
    float sampled_roughness = roughness_texture.Sample(material_sampler, input.texcoord).r;

    sampled_normal = ReconstructNormalBC5(sampled_normal.xy);
    ApplyDetailMaps(material, material_sampler, input.texcoord, sampled_albedo, sampled_normal);

    float3x3 TBN = float3x3(input.tangent, input.bitangent, input.normal);
    float3 normal = normalize(mul(sampled_normal.xyz, TBN));

    float4 albedo = material.mAlbedo * sampled_albedo;
    float metallic = material.mMetallic * sampled_metallic;
    float roughness = material.mRoughness * sampled_roughness;
    float3 emissive = material.mEmissive.rgb * sampled_emissive;

    uint4 packed = uint4(0, 0, 0, 0);
    PackAlbedo(albedo.rgb, packed);
    PackAlpha(albedo.a, packed);
    PackNormal(normal, packed);
    PackEmissive(emissive, packed);
    PackMetallicRoughness(metallic, roughness, packed);

    // Node 4 -> ProcedureShaderNode
    float3 outNormal0;
    Procedure0(float3(input.texcoord.xy, 0.0), fc.mTime, normal.rgb, outNormal0);
    // Node 5 -> VectorValueShaderNode
    float3 VectorValue_Out1 = float3(0.018, 0.115, 0.150);
    // Node 6 -> VectorValueShaderNode
    float3 VectorValue_Out2 = float3(0.040, 0.040, 0.000);
    // Node 7 -> VectorOpShaderNode
    float3 VectorOp_Out3 = float3(input.texcoord.xy, 0.0) * VectorValue_Out2;
    // Node 8 -> GradientNoiseShaderNode
    float outNoise4;
    Procedure4(VectorOp_Out3, outNoise4);
    // Node 9 -> FloatOpShaderNode
    float FloatOp_Out5 = float(outNoise4 * 0.025);
    // Node 10 -> VectorOpShaderNode
    float3 VectorOp_Out6 = VectorValue_Out1 + FloatOp_Out5;
    // Node 11 -> FloatValueShaderNode
    float FloatValue_Out7 = 0.050;
    // Node 0 -> PixelShaderOutputShaderNode
    PackAlbedo(VectorOp_Out6, packed);
    PackNormal(outNormal0, packed);
    PackRoughness(FloatValue_Out7, packed);


    output.gbuffer = asfloat(packed);

    float2 curr_pos = (input.curr_position.xyz / input.curr_position.w).xy - fc.mJitter;
    float2 prev_pos = (input.prev_position.xyz / input.prev_position.w).xy - fc.mPrevJitter;

    output.motionvectors = (curr_pos - prev_pos);
    output.motionvectors.xy *= float2(0.5, -0.5);

    output.selection = geometry.mEntity;

    return output;
}
