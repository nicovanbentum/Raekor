#pragma once

#include "pch.h"

inline void ThrowIfFailed(VkResult result) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error("VkResult was not VK_SUCCESS.");
    }
}
