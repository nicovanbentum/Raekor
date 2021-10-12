#pragma once

#include "pch.h"

inline void ThrowIfFailed(VkResult result) {
    if (result != VK_SUCCESS) {
        __debugbreak();
    }
}
