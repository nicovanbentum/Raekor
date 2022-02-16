struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 uv : TEXCOORD;
};

TextureCube obj_texture : TEXTURE : register(t0);
SamplerState obj_sampler_state : SAMPLER : register(s0);

float4 main(PS_INPUT input) : SV_Target
{

    return obj_texture.Sample(obj_sampler_state, input.uv);
}