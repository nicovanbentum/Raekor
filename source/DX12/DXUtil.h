#pragma once

#include "pch.h"

inline void gThrowIfFailed(HRESULT result) {
    if (FAILED(result)) {
        __debugbreak();
    }
}
