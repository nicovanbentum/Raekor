#pragma once

#include "Raekor/util.h"

namespace Raekor::DX12 {

/* explicit bindings to root descriptors hardcoded into the root signature. */
enum EBindSlot
{
    SRV0 = 1, SRV1, Count
};

// Total number of frames-in-flight, aka the number of backbuffers in the swapchain
static constexpr uint32_t sFrameCount = 2;
// Alignment constants as per DX12 spec
static constexpr uint32_t sConstantBufferAlignment = 256;
static constexpr uint32_t sByteAddressBufferAlignment = 4;
// Hardcoded limit for the number of RTVs / DSVs. Surely 255 should be more than enough..
static constexpr uint32_t sMaxRTVHeapSize = 0xFF;
static constexpr uint32_t sMaxDSVHeapSize = 0xFF;
// Hardcoded limit for static samplers as per DX12 spec, also used for regular sampler heap here.
static constexpr uint32_t sMaxSamplerHeapSize = 2043;
// Hardcoded limit for resource heap size as per DX12 hardware Tier 2, should be more than enough..
static constexpr uint32_t sMaxResourceHeapSize = 1'000'000;
// Hardcoded limit for root signature size as per DX12 spec
static constexpr uint32_t sMaxRootSignatureSize = 64 * sizeof(DWORD);
// Hardcoded limit for root constants, subtract root descriptors as they take 2 DWORDs each. Should match shared.h
static constexpr uint32_t sMaxRootConstantsSize = sMaxRootSignatureSize - ( EBindSlot::Count * 2 * sizeof(DWORD) );


inline void gThrowIfFailed(HRESULT inResult)
{
    if (FAILED(inResult))
        __debugbreak();
}


inline void gThrowIfFailed(FfxErrorCode inErrCode)
{
    if (inErrCode != FFX_OK)
        __debugbreak();
}

inline void gThrowIfFailed(NVSDK_NGX_Result inErrCode)
{
    if (inErrCode != NVSDK_NGX_Result_Success)
        __debugbreak();
}


inline void gThrowIfFailed(xess_result_t inErrCode)
{
    if (inErrCode != XESS_RESULT_SUCCESS)
        __debugbreak();
}


inline std::string gGetDebugName(ID3D12Resource* inResource)
{
    wchar_t name[128] = {};
    UINT size = sizeof(name);
    inResource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);
    return gWCharToString(name);
}


inline std::string gGetShaderISA(ID3D12PipelineState* inPipeline)
{
    UINT size = 0;
    inPipeline->GetPrivateData(WKPDID_CommentStringW, &size, NULL);

    if (size > 0)
    {
        std::wstring isa;
        isa.resize(size);

        inPipeline->GetPrivateData(WKPDID_CommentStringW, &size, isa.data());

        return gWCharToString(isa.c_str());
    }

    return {};
}


inline uint32_t gSpdCaculateMipCount(const uint32_t inWidth, const uint32_t inHeight)
{
    uint32_t max_res = glm::max(inWidth, inHeight);
    return uint32_t(( glm::min(glm::floor(glm::log2(float(max_res))), float(12)) ));
}


inline bool gIsDepthFormat(DXGI_FORMAT inFormat)
{
    switch (inFormat)
    {
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_D16_UNORM:
            return true;
        default:
            return false;
    }

    return false;
}


inline DXGI_FORMAT gGetDepthFormatSRV(DXGI_FORMAT inFormat)
{
    switch (inFormat)
    {
        case DXGI_FORMAT::DXGI_FORMAT_D16_UNORM:
            return  DXGI_FORMAT::DXGI_FORMAT_R16_FLOAT;

        case DXGI_FORMAT::DXGI_FORMAT_D24_UNORM_S8_UINT:
            return DXGI_FORMAT::DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

        case DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT:
            return DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT;

        case DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            return DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    }

    return DXGI_FORMAT_UNKNOWN;
}


} // namespace Raekor::DX12

