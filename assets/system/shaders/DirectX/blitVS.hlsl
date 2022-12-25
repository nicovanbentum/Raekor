struct VS_OUTPUT {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

struct RootConstants {
    uint source_index;
    uint sampler_index;
};

ConstantBuffer<RootConstants> rc : register(b0, space0);

SamplerState SamplerPointWrap       : register(s0);
SamplerState SamplerPointClamp      : register(s1);
SamplerState SamplerLinearWrap      : register(s2);
SamplerState SamplerLinearClamp     : register(s3);
SamplerState SamplerAnisoWrap       : register(s4);
SamplerState SamplerAnisoClamp      : register(s5);


VS_OUTPUT main(uint vertex_id : SV_VertexID) {
    VS_OUTPUT output;

    float x = float(((uint(vertex_id) + 2u) / 3u) % 2u);
    float y = float(((uint(vertex_id) + 1u) / 3u) % 2u);

    output.pos = float4(x * 2.0 - 1.0, y * 2.0 - 1.0, 1.0f, 1.0f);
    output.uv = float2(x, 1.0 - y);

    return output;
}

