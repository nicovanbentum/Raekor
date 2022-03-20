struct VS_OUTPUT {
    float2 texcoord : TEXCOORD;
    float4 position : SV_Position;
    float4 worldPosition : POSITIONT;
    float3 normal : NORMAL;
};

struct PS_OUTPUT {
    float4 albedo: SV_Target0;
};

struct RootConstants {
    float4 albedo;
    uint4 textures;
    float4x4 view_proj;
};

ConstantBuffer<RootConstants> root_constants : register(b0, space0);
SamplerState static_sampler : register(s0);

//[RootSignature(GLOBAL_ROOT_SIGNATURE)]
PS_OUTPUT main(in VS_OUTPUT input) {
    PS_OUTPUT output;

    Texture2D<float4> albedo_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.textures.x)];
    float3 albedo = root_constants.albedo.rgb;

     if(root_constants.textures.x > 0)
         albedo *= albedo_texture.SampleLevel(static_sampler, input.texcoord, 0).rgb;

    output.albedo = float4(albedo, 1.0);

    return output;
}