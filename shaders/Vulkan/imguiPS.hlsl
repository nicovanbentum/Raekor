#include "include/imgui.hlsli"

[[vk::push_constant]]
ConstantBuffer<PushConstants> pc;

[[vk::binding(0, 0)]]
Texture2D textures[];

[[vk::binding(0, 0)]]
SamplerState samplers[];

float4 main(VS_OUTPUT input) : SV_TARGET {
    float4 sampled = float4(1.0, 1.0, 1.0, 1.0);

    if(pc.textureIndex > -1) {
        Texture2D texture = textures[pc.textureIndex];
        SamplerState textureSampler = samplers[pc.textureIndex];
        sampled = texture.Sample(textureSampler, input.uv);
    }

    sampled.a = smoothstep(0.0, 1.0, sampled.r);
    sampled = float4(1, 1, 1, sampled.a);

    return input.color * sampled;
}