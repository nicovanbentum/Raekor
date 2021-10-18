#include "include/imgui.hlsli"

[[vk::push_constant]]
ConstantBuffer<PushConstants> pc;

VS_OUTPUT main(in VS_INPUT input, out float4 pos : SV_Position) {
    pos = mul(pc.proj, float4(input.pos, 0.0, 1.0));
    pos.z = (pos.z / pos.w * 0.5 + 0.5) * pos.w;

    VS_OUTPUT output;
    output.uv = input.uv;

    output.color.w = float((input.color & uint(0xff000000)) >> 24) / 255;
	output.color.z = float((input.color & uint(0x00ff0000)) >> 16) / 255;
	output.color.y = float((input.color & uint(0x0000ff00)) >> 8) / 255;
	output.color.x = float((input.color & uint(0x000000ff)) >> 0) / 255;

    return output;
}

/*
 dxc -spirv -T vs_6_7 -E main imguiVS.hlsl -Fo imguiVS.hlsl.bin
*/

