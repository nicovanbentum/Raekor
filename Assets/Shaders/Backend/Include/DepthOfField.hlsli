#ifndef DEPTH_OF_FIELD_HLSLI
#define DEPTH_OF_FIELD_HLSLI

#include "Bindless.hlsli"

#define DOF_SAMPLE_COUNT 48
#define DOF_GOLDEN_ANGLE 2.39996323

float DoFLinearizeDepth(float inDepth, DepthOfFieldRootConstants inConstants)
{
    return inConstants.mNearPlane * inConstants.mFarPlane / (inConstants.mFarPlane + inDepth * (inConstants.mNearPlane - inConstants.mFarPlane));
}


float DoFGetFocusDistance(DepthOfFieldRootConstants inConstants, Texture2D<float> inDepthTexture)
{
    if (!inConstants.mAutoFocus)
        return inConstants.mFocusDistance;

    return DoFLinearizeDepth(inDepthTexture.SampleLevel(SamplerPointClamp, float2(0.5, 0.5), 0), inConstants);
}


float DoFGetCoC(float inLinearDepth, float inFocusDistance, DepthOfFieldRootConstants inConstants)
{
    float coc = inConstants.mLensCoefficient * (inLinearDepth - inFocusDistance) / (inLinearDepth * max(inFocusDistance - inConstants.mFocalLength, 1e-4f));
    return clamp(coc, -inConstants.mMaxCoC, inConstants.mMaxCoC);
}

#endif // DEPTH_OF_FIELD_HLSLI
