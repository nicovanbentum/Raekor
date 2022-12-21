#include "include/shared.h"
#include "include/common.hlsli"
#include "include/packing.hlsli"

struct VS_OUTPUT {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

ROOT_CONSTANTS(LightingRootConstants, rc)

float map(float value, float start1, float stop1, float start2, float stop2) {
      return start2 + (stop2 - start2) * ((value - start1) / (stop1 - start1));
}

ByteAddressBuffer buffer;

float4 main(in VS_OUTPUT input) : SV_Target0 {
    Texture2D<uint4> gbuffer_texture = ResourceDescriptorHeap[rc.mGbufferRenderTexture];
    Texture2D<float4> shadow_texture = ResourceDescriptorHeap[rc.mShadowMaskTexture];
    Texture2D<float> depth_texture   = ResourceDescriptorHeap[rc.mGbufferDepthTexture];

    float4 shadow = shadow_texture[input.pos.xy];
    uint4 packed = asuint(gbuffer_texture[input.pos.xy]);

    uint frame_index = 1;
    uint offset = sizeof(FrameConstants) * frame_index;
    FrameConstants fc = buffer.Load<FrameConstants>(offset);
    
    float4 albedo = UnpackAlbedo(packed.x);
    float3 normal = UnpackNormal(packed.y);
    float metalness, roughness;
     UnpackMetallicRoughness(packed.z, metalness, roughness);

    float3 color = albedo.rgb * shadow.x + (albedo.rgb * 0.2); 
    color = pow(color, 1.0 / 2.2);

    return float4(color, 1.0);
}