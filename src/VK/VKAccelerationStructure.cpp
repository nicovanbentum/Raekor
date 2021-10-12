#include "pch.h"
#include "VKAccelerationStructure.h"

#include "VKUtil.h"
#include "VKDevice.h"
#include "VKExtensions.h"

namespace Raekor::VK {

void AccelerationStructure::create(Device& device, VkAccelerationStructureBuildGeometryInfoKHR& buildInfo, const uint32_t primitiveCount) {
    // get the acceleration structure sizes
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    
    EXT::vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primitiveCount, &sizeInfo);

    buffer = device.createBuffer(
        sizeInfo.accelerationStructureSize, 
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, 
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    // create the acceleration structure buffer
    VkAccelerationStructureCreateInfoKHR asInfo = {};
    asInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    asInfo.type = buildInfo.type;
    asInfo.size = sizeInfo.accelerationStructureSize;
    asInfo.buffer = buffer.buffer;

    ThrowIfFailed(EXT::vkCreateAccelerationStructureKHR(device, &asInfo, nullptr, &accelerationStructure));

    buildInfo.dstAccelerationStructure = accelerationStructure;

    Buffer scratchBuffer = device.createBuffer(
        sizeInfo.buildScratchSize, 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    buildInfo.scratchData.deviceAddress = device.getDeviceAddress(scratchBuffer);

    // build the acceleration structure on the GPU
    VkCommandBuffer commandBuffer = device.startSingleSubmit();

    VkAccelerationStructureBuildRangeInfoKHR buildRange = {};
    buildRange.primitiveCount = primitiveCount;

    const std::array buildRanges = { &buildRange };

    EXT::vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, buildRanges.data());

    device.flushSingleSubmit(commandBuffer);

    device.destroyBuffer(scratchBuffer);
}

void AccelerationStructure::destroy(Device& device) {
    if (accelerationStructure) {
        EXT::vkDestroyAccelerationStructureKHR(device, accelerationStructure, nullptr);
    }

    device.destroyBuffer(buffer);
}

}