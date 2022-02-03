struct VS_OUTPUT {
    float2 texcoord : TEXCOORD;
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

//Texture2D<float4> textures[] : register(t0, space1);
//ByteAddressBuffer buffers[] : register(b0, space2);

PS_OUTPUT main(in VS_OUTPUT input) {
    PS_OUTPUT output;
    output.albedo = float4(1.0, 1.0, 1.0, 1.0);
    return output;
}