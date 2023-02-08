#pragma once

#include "Raekor/util.h"

namespace Raekor::DX12 {

/* explicit bindings to root descriptors hardcoded into the root signature. */
enum EBindSlot {
	    CBV0 = 1, CBV1, CBV2, CBV3,
		SRV0, SRV1, SRV2, SRV3,
		UAV0, UAV1, UAV2, UAV3, Count
};

static constexpr uint32_t sFrameCount = 2;
static constexpr uint32_t sConstantBufferAlignment = 256;
static constexpr uint32_t sByteAddressBufferAlignment = 4;
static constexpr uint32_t sRootSignatureSize = 64 * sizeof(DWORD);
static constexpr uint32_t sMaxRootConstantsSize = 32 * sizeof(DWORD);


inline void gThrowIfFailed(HRESULT inResult) {
    if (FAILED(inResult))
        __debugbreak();
}


inline void gThrowIfFailed(FfxErrorCode inErrCode) {
    if (inErrCode != FFX_OK)
        __debugbreak();
}


inline std::string gGetDebugName(ID3D12Resource* inResource) {
	wchar_t name[128] = {};
	UINT size = sizeof(name);
	inResource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);
	return gWCharToString(name);
}

} // namespace Raekor::DX12

