#include "pch.h"
#include "DXUpscaler.h"

namespace Raekor::DX12 {

UVec2 Upscaler::sGetRenderResolution(UVec2 inDisplayResolution, EUpscalerQuality inQuality)
{
    auto display_res = Vec2(inDisplayResolution);

    switch (inQuality)
    {
        case UPSCALER_QUALITY_QUALITY: display_res /= 1.5f; break;
        case UPSCALER_QUALITY_BALANCED: display_res /= 1.7f; break;
        case UPSCALER_QUALITY_PERFORMANCE: display_res /= 2.0f; break;
        default: break;
    }

    return UVec2(glm::floor(display_res));
}



NVSDK_NGX_PerfQuality_Value Upscaler::sGetQuality(EUpscalerQuality inQuality)
{
    switch (inQuality)
    {
        case UPSCALER_QUALITY_NATIVE: return NVSDK_NGX_PerfQuality_Value_DLAA;
        case UPSCALER_QUALITY_QUALITY: return NVSDK_NGX_PerfQuality_Value_MaxQuality;
        case UPSCALER_QUALITY_BALANCED: return NVSDK_NGX_PerfQuality_Value_Balanced;
        case UPSCALER_QUALITY_PERFORMANCE: return NVSDK_NGX_PerfQuality_Value_MaxPerf;
        default: assert(false);
    }

    gUnreachableCode();
}


} // namespace Raekor::DX12