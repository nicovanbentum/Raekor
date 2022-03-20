
struct VS_OUTPUT {
    float2 texcoord : TEXCOORD;
    float4 position : SV_Position;
    float4 worldPosition : POSITIONT;
    float3 normal : NORMAL;
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
SamplerState static_sampler : register(s0);

VS_OUTPUT main(in VS_INPUT input) {
    VS_OUTPUT output;

    output.position = mul(root_constants.view_proj, float4(input.pos, 1.0));
    output.normal = input.normal;
    output.worldPosition = float4(input.pos, 1.0);

    output.texcoord = input.texcoord;

    return output;
}
