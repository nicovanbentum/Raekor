#pragma once

#include "Raekor/pch.h"

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#define VK_ENABLE_BETA_EXTENSIONS
#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"
#include "spirv_reflect.h"
