#include "include/common.hlsli"
#include "include/packing.hlsli"

struct VS_OUTPUT {
    float4 mPos : SV_Position;
    float3 mPrevPos : PrevPosition;
    float3 mNormal : NORMAL;
    float height : Field0;
};

struct RenderTargets {
    float4 GBufferRT0 : SV_Target0;
};

RenderTargets main(VS_OUTPUT input)
{
    float4 albedo = float4(0.0, lerp(0, 1, input.height), 0.0, 1.0);
    
    uint4 packed = uint4(0, 0, 0, 0);
    PackAlbedo(albedo.rgb, packed);
    PackAlpha(albedo.a, packed);
    PackNormal(input.mNormal, packed);
    PackMetallicRoughness(0, 1.0, packed);
    
    RenderTargets mrt;
    mrt.GBufferRT0 = asfloat(packed);
    return mrt;
}