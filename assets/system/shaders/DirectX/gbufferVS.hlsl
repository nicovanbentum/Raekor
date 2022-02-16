struct VS_OUTPUT {
    float2 texcoord : TEXCOORD;
    float4 pos : SV_Position;
};

struct VS_INPUT {
    float3 pos : POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

// #define GlobalRootSignature "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED )," \
//                             "StaticSampler(s0, addressU = TEXTURE_ADDRESS_CLAMP, filter = FILTER_MIN_MAG_MIP_LINEAR)"

// [RootSignature(GlobalRootSignature)]
VS_OUTPUT main(in VS_INPUT input) {
    VS_OUTPUT output;

    output.pos = float4(input.pos, 1.0); // TODO: matrix

    output.texcoord = input.texcoord;

    return output;
}
