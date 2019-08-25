struct PS_INPUT {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D obj_texture : TEXTURE : register(t0);
SamplerState obj_sampler_state : SAMPLER : register(s0);

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 pixel_color = obj_texture.Sample(obj_sampler_state, input.uv);
    return pixel_color;
}