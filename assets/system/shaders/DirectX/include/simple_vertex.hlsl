struct VS_INPUT {
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct VS_OUTPUT {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

cbuffer myCbuffer : register(b0) {
    float4x4 m;
    float4x4 v;
    float4x4 p;
}

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float4 worldPos = mul(m, float4(input.pos, 1.0));
    output.pos = mul(mul(p, v), worldPos);
    output.uv = input.uv;
    return output;
}