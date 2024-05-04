#include "pch.h"
#include "VKExtensions.h"

namespace RK {

void EXT::sInit(VkDevice device)
{
	vkCmdTraceRaysKHR = VK_LOAD_FN(device, vkCmdTraceRaysKHR);
	vkCreateRayTracingPipelinesKHR = VK_LOAD_FN(device, vkCreateRayTracingPipelinesKHR);
	vkCreateAccelerationStructureKHR = VK_LOAD_FN(device, vkCreateAccelerationStructureKHR);
	vkDestroyAccelerationStructureKHR = VK_LOAD_FN(device, vkDestroyAccelerationStructureKHR);
	vkCmdBuildAccelerationStructuresKHR = VK_LOAD_FN(device, vkCmdBuildAccelerationStructuresKHR);
	vkGetRayTracingShaderGroupHandlesKHR = VK_LOAD_FN(device, vkGetRayTracingShaderGroupHandlesKHR);
	vkGetAccelerationStructureBuildSizesKHR = VK_LOAD_FN(device, vkGetAccelerationStructureBuildSizesKHR);
	vkGetAccelerationStructureDeviceAddressKHR = VK_LOAD_FN(device, vkGetAccelerationStructureDeviceAddressKHR);
	vkCmdPipelineBarrier2KHR = VK_LOAD_FN(device, vkCmdPipelineBarrier2KHR);
}

}