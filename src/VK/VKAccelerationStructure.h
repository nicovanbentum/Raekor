#pragma once

#include "VKDevice.h"

namespace Raekor::VK {

struct AccelerationStructure {
    void create(Device& device, VkAccelerationStructureBuildGeometryInfoKHR& buildInfo, const uint32_t primitiveCount);
    void destroy(Device& device);

    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
};

}
