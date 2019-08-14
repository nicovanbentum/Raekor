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
    float4x4 MVP;
}

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.pos = mul(float4(input.pos, 1.0f), MVP).xyww;
    output.uv = input.pos;
    return output;
}