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

    // create the buffer that stores the acceleration structure
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeInfo.accelerationStructureSize;
    bufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    ThrowIfFailed(vmaCreateBuffer(device.getAllocator(), &bufferInfo, &allocCreateInfo, &buffer, &allocation, nullptr));

    // create the acceleration structure
    VkAccelerationStructureCreateInfoKHR asInfo = {};
    asInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    asInfo.type = buildInfo.type;
    asInfo.size = sizeInfo.accelerationStructureSize;
    asInfo.buffer = buffer;

    ThrowIfFailed(EXT::vkCreateAccelerationStructureKHR(device, &asInfo, nullptr, &accelerationStructure));

    buildInfo.dstAccelerationStructure = accelerationStructure;

    // create the scratch buffer for writing
    VkBufferCreateInfo scratchBufferInfo = {};
    scratchBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    scratchBufferInfo.size = sizeInfo.buildScratchSize;
    scratchBufferInfo.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VkBuffer scratchBuffer;
    VmaAllocation scratchAlloc;

    ThrowIfFailed(vmaCreateBuffer(device.getAllocator(), &scratchBufferInfo, &allocCreateInfo, &scratchBuffer, &scratchAlloc, nullptr));

    buildInfo.scratchData.deviceAddress = device.getDeviceAddress(scratchBuffer);

    // build the acceleration structure
    VkCommandBuffer commandBuffer = device.startSingleSubmit();

    VkAccelerationStructureBuildRangeInfoKHR buildRange = {};
    buildRange.primitiveCount = primitiveCount;

    std::array buildRanges = { &buildRange };

    EXT::vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, buildRanges.data());

    device.flushSingleSubmit(commandBuffer);

    vmaDestroyBuffer(device.getAllocator(), scratchBuffer, scratchAlloc);
}

void AccelerationStructure::destroy(Device& device) {
    if (accelerationStructure) {
        EXT::vkDestroyAccelerationStructureKHR(device, accelerationStructure, nullptr);
    }

    if (buffer && allocation) {
        vmaDestroyBuffer(device.getAllocator(), buffer, allocation);
    }
}

}