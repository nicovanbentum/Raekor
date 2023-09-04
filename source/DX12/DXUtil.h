#pragma once

#include "Raekor/util.h"

namespace Raekor::DX12 {

/* explicit bindings to root descriptors hardcoded into the root signature. */
enum EBindSlot
{
    SRV0 = 1, SRV1, Count
};

static constexpr uint32_t sFrameCount = 2;
static constexpr uint32_t sConstantBufferAlignment = 256;
static constexpr uint32_t sByteAddressBufferAlignment = 4;
static constexpr uint32_t sRootSignatureSize = 64 * sizeof(DWORD);
static constexpr uint32_t sMaxRootConstantsSize = sRootSignatureSize - ( EBindSlot::Count * 2 * sizeof(DWORD) );


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


inline std::string gGetDebugName(ID3D12Resource* inResource)
{
    wchar_t name[128] = {};
    UINT size = sizeof(name);
    inResource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);
    return gWCharToString(name);
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

