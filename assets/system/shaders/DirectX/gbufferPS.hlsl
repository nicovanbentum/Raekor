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

struct PS_OUTPUT {
    float4 albedo: SV_Target0;
};

// #define GlobalRootSignature "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED )," \
//                             "StaticSampler(s0, addressU = TEXTURE_ADDRESS_CLAMP, filter = FILTER_MIN_MAG_MIP_LINEAR)"

// [RootSignature(GlobalRootSignature)]
PS_OUTPUT main(in VS_OUTPUT input) {
    PS_OUTPUT output;

    output.albedo = float4(1.0, 1.0, 1.0, 1.0);

    return output;
}