#include "include/common.hlsli"
#include "include/packing.hlsli"

struct VS_OUTPUT {
    float4 position : SV_Position;
    float height : Field0;
};

struct PS_OUTPUT {
    float4 gbuffer : SV_Target0;
};

PS_OUTPUT main(VS_OUTPUT input) {
    PS_OUTPUT output;
    
    uint4 packed = uint4(0, 0, 0, 0);
    
    float4 albedo = float4(0.0, lerp(0, 1, input.height), 0.0, 1.0);
    
    packed.x = PackAlbedo(albedo);
    packed.y = PackNormal(float3(1.0, 0.0, 0.0));
    packed.z = PackMetallicRoughness(0, 1.0);
    
    output.gbuffer = asfloat(packed);
    return output;
}