#pragma once

#include "Raekor/util.h"

namespace Raekor::DX12 {

/* explicit bindings to root descriptors hardcoded into the root signature. */
enum EBindSlot
{
    Start = 1, 
    CBV0 = Start, 
    SRV0,
    SRV1, 
    Count
};

// Total number of frames-in-flight, aka the number of backbuffers in the swapchain
static constexpr uint32_t sFrameCount = 2;
// Alignment constants as per DX12 spec
static constexpr uint32_t sConstantBufferAlignment = 256;
static constexpr uint32_t sByteAddressBufferAlignment = 4;
// Hardcoded limit for the number of RTVs / DSVs. Surely 255 should be more than enough..
static constexpr uint32_t sMaxRTVHeapSize = 0xFF;
static constexpr uint32_t sMaxDSVHeapSize = 0xFF;
// Hardcoded limit for the number of UAV clear descriptors, surely 255 should be enough..
static constexpr uint32_t sMaxClearHeapSize = 0xFF;
// Hardcoded limit for static samplers as per DX12 spec, also used for regular sampler heap here.
static constexpr uint32_t sMaxSamplerHeapSize = 2043;
// Hardcoded limit for resource heap size as per DX12 hardware Tier 2, should be more than enough..
static constexpr uint32_t sMaxResourceHeapSize = 1'000'000 - sMaxClearHeapSize;
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
    char name[1024];
    uint32_t size = 0;

    inResource->GetPrivateData(WKPDID_D3DDebugObjectName, &size, nullptr);
    inResource->GetPrivateData(WKPDID_D3DDebugObjectName, &size, name);

    return std::string(name, size - 1);
}


inline void gSetDebugName(ID3D12Resource* inResource, const char* inName)
{
    inResource->SetPrivateData(WKPDID_D3DDebugObjectName, (uint32_t)strlen(inName) + 1, inName);
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


inline size_t gBitsPerPixel(DXGI_FORMAT fmt) noexcept
{
    switch (static_cast<int>( fmt ))
    {
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT:
            return 128;

        case DXGI_FORMAT_R32G32B32_TYPELESS:
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT:
            return 96;

        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_SINT:
        case DXGI_FORMAT_R32G32_TYPELESS:
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        case DXGI_FORMAT_Y416:
        case DXGI_FORMAT_Y210:
        case DXGI_FORMAT_Y216:
            return 64;

        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT:
        case DXGI_FORMAT_R11G11B10_FLOAT:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_SINT:
        case DXGI_FORMAT_R16G16_TYPELESS:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SNORM:
        case DXGI_FORMAT_R16G16_SINT:
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_SINT:
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
        case DXGI_FORMAT_R8G8_B8G8_UNORM:
        case DXGI_FORMAT_G8R8_G8B8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        case DXGI_FORMAT_AYUV:
        case DXGI_FORMAT_Y410:
        case DXGI_FORMAT_YUY2:
            return 32;

        case DXGI_FORMAT_P010:
        case DXGI_FORMAT_P016:
            return 24;

        case DXGI_FORMAT_R8G8_TYPELESS:
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R8G8_SNORM:
        case DXGI_FORMAT_R8G8_SINT:
        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SNORM:
        case DXGI_FORMAT_R16_SINT:
        case DXGI_FORMAT_B5G6R5_UNORM:
        case DXGI_FORMAT_B5G5R5A1_UNORM:
        case DXGI_FORMAT_A8P8:
        case DXGI_FORMAT_B4G4R4A4_UNORM:
            return 16;

        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_420_OPAQUE:
        case DXGI_FORMAT_NV11:
            return 12;

        case DXGI_FORMAT_R8_TYPELESS:
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_SNORM:
        case DXGI_FORMAT_R8_SINT:
        case DXGI_FORMAT_A8_UNORM:
        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_BC6H_TYPELESS:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_TYPELESS:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
        case DXGI_FORMAT_AI44:
        case DXGI_FORMAT_IA44:
        case DXGI_FORMAT_P8:
            return 8;

        case DXGI_FORMAT_R1_UNORM:
            return 1;

        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            return 4;

        default:
            return 0;
    }
}


inline HRESULT gComputePitch(DXGI_FORMAT fmt, size_t width, size_t height,
    size_t& rowPitch, size_t& slicePitch)
{
    uint64_t pitch = 0;
    uint64_t slice = 0;

    switch (static_cast<int>( fmt ))
    {
        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
        {
            const uint64_t nbw = std::max<uint64_t>(1u, ( uint64_t(width) + 3u ) / 4u);
            const uint64_t nbh = std::max<uint64_t>(1u, ( uint64_t(height) + 3u ) / 4u);
            pitch = nbw * 8u;
            slice = pitch * nbh;
            break;
        }

        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_BC6H_TYPELESS:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_TYPELESS:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
        {
            const uint64_t nbw = std::max<uint64_t>(1u, ( uint64_t(width) + 3u ) / 4u);
            const uint64_t nbh = std::max<uint64_t>(1u, ( uint64_t(height) + 3u ) / 4u);
            pitch = nbw * 16u;
            slice = pitch * nbh;
            break;
        }

        case DXGI_FORMAT_R8G8_B8G8_UNORM:
        case DXGI_FORMAT_G8R8_G8B8_UNORM:
        case DXGI_FORMAT_YUY2:
            pitch = ( ( uint64_t(width) + 1u ) >> 1 ) * 4u;
            slice = pitch * uint64_t(height);
            break;

        default:
            {
                size_t bpp = gBitsPerPixel(fmt);
                if (!bpp)
                    return E_INVALIDARG;

                // Default byte alignment
                pitch = ( uint64_t(width) * bpp + 7u ) / 8u;
                slice = pitch * uint64_t(height);
                break;
            }
    }

    rowPitch = static_cast<size_t>( pitch );
    slicePitch = static_cast<size_t>( slice );

    return S_OK;
}

} // namespace Raekor::DX12

