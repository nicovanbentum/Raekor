#pragma once

#ifndef VK_LOAD_FN
#define VK_LOAD_FN(device, x) PFN_##x(vkGetDeviceProcAddr(device, #x))
#endif

namespace Raekor {

class EXT
{
public:
	inline static PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
	inline static PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
	inline static PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
	inline static PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
	inline static PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
	inline static PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
	inline static PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
	inline static PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
	inline static PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR;

	static void sInit(VkDevice device);
};



}
