#include "include/common.hlsli"
#include "include/packing.hlsli"

struct VS_OUTPUT {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

struct RootConstants {
    uint source_index;
    uint sampler_index;
};

ROOT_CONSTANTS(RootConstants, root_constants)

float4 main(in VS_OUTPUT input) : SV_Target0 {
    Texture2D<float4> src = ResourceDescriptorHeap[root_constants.source_index];
    uint4 packed = asuint(src[input.pos.xy]);
    float4 albedo = RGBA8ToFloat4(packed.x);
    
    return float4(albedo.rgb, 1.0);
}