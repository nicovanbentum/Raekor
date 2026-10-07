#include "Include/Bindless.hlsli"
#include "Include/Common.hlsli"

ROOT_CONSTANTS(SDFUIRootConstants, rc)

struct VS_OUTPUT
{
    float4 mPosition : SV_Position;
    float2 mPixel    : PIXEL;
    nointerpolation uint mPrimitive : PRIMITIVE;
};

VS_OUTPUT main(uint inVertexID : SV_VertexID, uint inInstanceID : SV_InstanceID)
{
    StructuredBuffer<UIPrimitive> primitives = ResourceDescriptorHeap[rc.mPrimitivesBuffer];
    UIPrimitive primitive = primitives[inInstanceID];

    static const float2 cCorners[6] = { float2(0, 0), float2(1, 0), float2(0, 1), float2(0, 1), float2(1, 0), float2(1, 1) };

    const float expand = primitive.mType == UI_PRIMITIVE_GLYPH ? 0.0 : primitive.mSoftness + 1.0;
    const float2 rect_min = primitive.mRect.xy - expand;
    const float2 rect_max = primitive.mRect.zw + expand;

    const float2 pixel = lerp(rect_min, rect_max, cCorners[inVertexID]);
    const float2 ndc = pixel * rc.mInvRenderSize * float2(2.0, -2.0) + float2(-1.0, 1.0);

    VS_OUTPUT output;
    output.mPosition = float4(ndc, 0.0, 1.0);
    output.mPixel = pixel;
    output.mPrimitive = inInstanceID;
    return output;
}
