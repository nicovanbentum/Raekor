struct VS_INPUT
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float3 uv : TEXCOORD;
};

cbuffer myCbuffer : register(b0)
{
    float4x4 m;
    float4x4 v;
    float4x4 p;
}

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float4x4 v2 = v;
    v2[0][3] = 0;
    v2[1][3] = 0;
    v2[2][3] = 0;
    v2[3][3] = 0;
    float4 worldPos = mul(m, float4(input.pos, 1.0));
    output.pos = mul(mul(p, v2), worldPos);
    output.uv = input.pos;
    return output;
}