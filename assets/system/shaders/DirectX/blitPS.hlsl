#include "include/common.hlsli"
#include "include/packing.hlsli"

struct VS_OUTPUT {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

struct RootConstants {
    uint source_index;
    uint gbuffer_index;
    uint sampler_index;
};

ROOT_CONSTANTS(RootConstants, root_constants)

float map(float value, float start1, float stop1, float start2, float stop2) {
      return start2 + (stop2 - start2) * ((value - start1) / (stop1 - start1));
}

float4 main(in VS_OUTPUT input) : SV_Target0 {
    Texture2D<float4> src = ResourceDescriptorHeap[root_constants.source_index];
    Texture2D<uint4> gbuffer = ResourceDescriptorHeap[root_constants.gbuffer_index];

    float4 shadow = src[input.pos.xy];
     uint4 packed = asuint(gbuffer[input.pos.xy]);
    
     float4 albedo = UnpackAlbedo(packed.x);
     float3 normal = UnpackNormal(packed.y);
    
     float metalness, roughness;
     UnpackMetallicRoughness(packed.z, metalness, roughness);

    float3 color = albedo.rgb * shadow.x + (albedo.rgb * 0.2); 
    color = pow(color, 1.0 / 2.2);

    return float4(color, 1.0);
}