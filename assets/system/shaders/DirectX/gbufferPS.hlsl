#include "include/common.hlsli"
#include "include/packing.hlsli"

struct VS_OUTPUT {
    float2 texcoord : TEXCOORD;
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
};

struct PS_OUTPUT {
    float4 gbuffer: SV_Target0;
};

struct RootConstants {
    float4 albedo;
    uint4 textures;
    float4x4 view_proj;
};

ROOT_CONSTANTS(RootConstants, root_constants)

//[RootSignature(GLOBAL_ROOT_SIGNATURE)]
PS_OUTPUT main(in VS_OUTPUT input) {
    PS_OUTPUT output;

    float4 albedo_sample = float4(1.0, 1.0, 1.0, 1.0);

    Texture2D<float4> albedo_texture = ResourceDescriptorHeap[root_constants.textures.x];
    float4 albedo = root_constants.albedo * albedo_texture.SampleLevel(SamplerPointWrap, input.texcoord, 0.0);

    uint4 packed = uint4(0, 0, 0, 0);
    packed.x = Float4ToRGBA8(albedo);

    output.gbuffer = asfloat(packed);

    return output;
}