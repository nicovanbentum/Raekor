struct VS_OUTPUT {
    float2 texcoord : TEXCOORD;
};

struct VS_INPUT {
    float3 pos : POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

VS_OUTPUT main(in VS_INPUT input, out float4 pos : SV_Position) {
    VS_OUTPUT output;

    pos = float4(input.pos, 1.0); // TODO: matrix

    output.texcoord = input.texcoord;

    return output;
}