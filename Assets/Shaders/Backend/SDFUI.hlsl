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
    float4 color = primitive.mColor;

    float2 local = inParams.mPixel - center;

    if (primitive.mRotation != 0.0)
    {
        const float s = sin(-primitive.mRotation);
        const float c = cos(-primitive.mRotation);
        local = float2(local.x * c - local.y * s, local.x * s + local.y * c);
    }

    if (primitive.mType == UI_PRIMITIVE_RECT || primitive.mType == UI_PRIMITIVE_RECT_GRADIENT_V || primitive.mType == UI_PRIMITIVE_RECT_GRADIENT_H)
    {
        float distance = RoundedBoxDistance(local, half_size, primitive.mRadius);

        if (primitive.mThickness > 0.0)
            distance = abs(distance + primitive.mThickness * 0.5) - primitive.mThickness * 0.5;

        coverage = Coverage(distance, primitive.mSoftness);

        if (primitive.mType == UI_PRIMITIVE_RECT_GRADIENT_V)
            color = lerp(primitive.mColor, primitive.mUVRect, saturate(local.y / max(half_size.y * 2.0, 1e-4) + 0.5));
        else if (primitive.mType == UI_PRIMITIVE_RECT_GRADIENT_H)
            color = lerp(primitive.mColor, primitive.mUVRect, saturate(local.x / max(half_size.x * 2.0, 1e-4) + 0.5));
    }
    else if (primitive.mType == UI_PRIMITIVE_ARC)
    {
        const float radius = length(local);
        const float distance = abs(radius - primitive.mRadius) - primitive.mThickness * 0.5;
        coverage = Coverage(distance, primitive.mSoftness);

        if (primitive.mUVRect.y < 1.0)
        {
            const float angle = frac(( atan2(local.y, local.x) - primitive.mUVRect.x ) / 6.28318530718 + 1.0);
            const float arc_length = angle * 6.28318530718 * max(radius, 1.0);
            const float arc_end = primitive.mUVRect.y * 6.28318530718 * max(radius, 1.0);
            coverage *= saturate(arc_end - arc_length + 0.5);
        }
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

    return float4(color.rgb, color.a * coverage);
}
