
#include "include/common.hlsli"

struct VS_OUTPUT {
    float2 texcoord : TEXCOORD;
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BINORMAL;
};

struct VS_INPUT {
    float3 pos : POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

struct RootConstants {
    float4 albedo;
    uint4 textures;
    float4 properties;
    float4x4 view_proj;
};

ROOT_CONSTANTS(RootConstants, root_constants)

VS_OUTPUT main(in VS_INPUT input) {
    VS_OUTPUT output;

    output.position = mul(root_constants.view_proj, float4(input.pos, 1.0));
    output.normal = input.normal;
    output.normal = normalize(input.normal);
	output.tangent = normalize(input.tangent);
    output.tangent = normalize(output.tangent - dot(output.tangent, output.normal) * output.normal);
	output.bitangent = cross(output.normal, output.tangent);
    output.texcoord = input.texcoord;

    return output;
}
