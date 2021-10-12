#pragma once

#include "VKStructs.h"

namespace Raekor::VK {

class Device;

struct AccelerationStructure {
    void create(Device& device, VkAccelerationStructureBuildGeometryInfoKHR& buildInfo, const uint32_t primitiveCount);
    void destroy(Device& device);

    Buffer buffer = {};
    VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
};

}
