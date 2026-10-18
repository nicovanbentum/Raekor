#include "Include/Bindless.hlsli"
#include "Include/Common.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(LuminanceAverageRootConstants, rc)

groupshared float histogram_shared[LUMINANCE_HISTOGRAM_BIN_COUNT];

[numthreads(LUMINANCE_HISTOGRAM_GROUP_SIZE, LUMINANCE_HISTOGRAM_GROUP_SIZE, 1)]
void main(uint inGroupIndex : SV_GroupIndex)
{
    RWByteAddressBuffer histogram_buffer = ResourceDescriptorHeap[rc.mHistogramBuffer];
    RWByteAddressBuffer exposure_buffer = ResourceDescriptorHeap[rc.mExposureBuffer];

    const float bin_count = float(histogram_buffer.Load(inGroupIndex * 4));
    histogram_shared[inGroupIndex] = bin_count * float(inGroupIndex);

    histogram_buffer.Store(inGroupIndex * 4, 0);

    GroupMemoryBarrierWithGroupSync();

    [unroll]
    for (uint stride = LUMINANCE_HISTOGRAM_BIN_COUNT >> 1; stride > 0; stride >>= 1)
    {
        if (inGroupIndex < stride)
            histogram_shared[inGroupIndex] += histogram_shared[inGroupIndex + stride];

        GroupMemoryBarrierWithGroupSync();
    }

    if (inGroupIndex != 0)
        return;

    const float black_pixel_count = bin_count;
    const float weighted_log_average = histogram_shared[0] / max(float(rc.mPixelCount) - black_pixel_count, 1.0) - 1.0;
    const float average_luminance = exp2(weighted_log_average / (LUMINANCE_HISTOGRAM_BIN_COUNT - 2) * rc.mLogLuminanceRange + rc.mMinLogLuminance);

    const float default_luminance = 1.0 / 9.6;
    const float previous_luminance = asfloat(exposure_buffer.Load(0));

    float adapted_luminance = default_luminance;

    if (fc.mFrameCounter == 1)
        adapted_luminance = average_luminance;
    else if (fc.mFrameCounter > 1 && isfinite(previous_luminance) && previous_luminance > 0.0)
        adapted_luminance = previous_luminance + (average_luminance - previous_luminance) * (1.0 - exp(-fc.mDeltaTime * rc.mAdaptationSpeed));

    const float exposure = exp2(rc.mExposureCompensation) / (9.6 * adapted_luminance);

    exposure_buffer.Store2(0, asuint(float2(adapted_luminance, exposure)));
}
