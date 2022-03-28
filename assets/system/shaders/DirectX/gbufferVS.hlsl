
struct VS_OUTPUT {
    float2 texcoord : TEXCOORD;
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
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
    float4x4 view_proj;
};

ConstantBuffer<RootConstants> root_constants : register(b0, space0);

SamplerState SamplerPointWrap       : register(s0);
SamplerState SamplerPointClamp      : register(s1);
SamplerState SamplerLinearWrap      : register(s2);
SamplerState SamplerLinearClamp     : register(s3);
SamplerState SamplerAnisoWrap       : register(s4);
SamplerState SamplerAnisoClamp      : register(s5);

VS_OUTPUT main(in VS_INPUT input) {
    VS_OUTPUT output;

    output.position = mul(root_constants.view_proj, float4(input.pos, 1.0));
    output.normal = input.normal;
    output.normal = normalize(input.normal);
	output.tangent = normalize(input.tangent);
    output.tangent = normalize(output.tangent - dot(output.tangent, output.normal) * output.normal);
	output.binormal = cross(output.normal, output.tangent);
    output.texcoord = input.texcoord;

    return output;
}
