#pragma once

#include "Raekor/util.h"

namespace Raekor::DX {

static constexpr uint32_t sRootSignatureSize = 64 * sizeof(DWORD);

inline void gThrowIfFailed(HRESULT result) {
    if (FAILED(result)) {
        __debugbreak();
    }
}

}

