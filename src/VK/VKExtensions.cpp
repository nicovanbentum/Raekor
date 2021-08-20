#include "pch.h"
#include "VKExtensions.h"

namespace Raekor {

void EXT::init(VkDevice device) {
    vkCreateRayTracingPipelinesKHR = VK_LOAD_FN(device, vkCreateRayTracingPipelinesKHR);
    vkCreateAccelerationStructureKHR = VK_LOAD_FN(device, vkCreateAccelerationStructureKHR);
    vkDestroyAccelerationStructureKHR = VK_LOAD_FN(device, vkDestroyAccelerationStructureKHR);
    vkCmdBuildAccelerationStructuresKHR = VK_LOAD_FN(device, vkCmdBuildAccelerationStructuresKHR);
    vkGetRayTracingShaderGroupHandlesKHR = VK_LOAD_FN(device, vkGetRayTracingShaderGroupHandlesKHR);
    vkGetAccelerationStructureBuildSizesKHR = VK_LOAD_FN(device, vkGetAccelerationStructureBuildSizesKHR);
    vkGetAccelerationStructureDeviceAddressKHR = VK_LOAD_FN(device, vkGetAccelerationStructureDeviceAddressKHR);
}

}