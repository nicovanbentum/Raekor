#pragma once

#include "Raekor/util.h"

namespace Raekor::DX {

/* explicit bindings to root descriptors hardcoded into the root signature. */
enum EBindSlot {
	SRV0 = 1, SRV1, SRV2, SRV3,
	    CBV0, CBV1, CBV2, CBV3,
		UAV0, UAV1, UAV2, UAV3, Count
};


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

}

