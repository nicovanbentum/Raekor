#include "Include/Bindless.hlsli"
#include "Include/Common.hlsli"

ROOT_CONSTANTS(SDFUIRootConstants, rc)

struct VS_OUTPUT
{
    float4 mPosition : SV_Position;
    float2 mPixel    : PIXEL;
    nointerpolation uint mPrimitive : PRIMITIVE;
};


float RoundedBoxDistance(float2 inPoint, float2 inHalfSize, float inRadius)
{
    const float2 q = abs(inPoint) - inHalfSize + inRadius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - inRadius;
}


float Coverage(float inDistance, float inSoftness)
{
    return 1.0 - smoothstep(-0.5, 0.5 + inSoftness, inDistance);
}


float4 main(in VS_OUTPUT inParams) : SV_Target0
{
    StructuredBuffer<UIPrimitive> primitives = ResourceDescriptorHeap[rc.mPrimitivesBuffer];
    UIPrimitive primitive = primitives[inParams.mPrimitive];

    const float2 center = ( primitive.mRect.xy + primitive.mRect.zw ) * 0.5;
    const float2 half_size = ( primitive.mRect.zw - primitive.mRect.xy ) * 0.5;

    float coverage = 0.0;

    if (primitive.mType == UI_PRIMITIVE_RECT)
    {
        float distance = RoundedBoxDistance(inParams.mPixel - center, half_size, primitive.mRadius);

        if (primitive.mThickness > 0.0)
            distance = abs(distance + primitive.mThickness * 0.5) - primitive.mThickness * 0.5;

        coverage = Coverage(distance, primitive.mSoftness);
    }
    else if (primitive.mType == UI_PRIMITIVE_CIRCLE)
    {
        const float distance = length(inParams.mPixel - center) - primitive.mRadius;
        coverage = Coverage(distance, primitive.mSoftness);
    }
    else if (primitive.mType == UI_PRIMITIVE_GLYPH)
    {
        Texture2D<float> font_atlas = ResourceDescriptorHeap[rc.mFontAtlasTexture];

        const float2 uv = lerp(primitive.mUVRect.xy, primitive.mUVRect.zw, ( inParams.mPixel - primitive.mRect.xy ) / max(primitive.mRect.zw - primitive.mRect.xy, 1e-4));
        const float sampled = font_atlas.SampleLevel(SamplerLinearClamp, uv, 0);

        const float atlas_distance = ( 0.5 - sampled ) * 2.0 * rc.mFontDistanceRange;
        const float screen_distance = atlas_distance * primitive.mRadius;

        coverage = Coverage(screen_distance, 0.0);
    }

    return float4(primitive.mColor.rgb, primitive.mColor.a * coverage);
}
