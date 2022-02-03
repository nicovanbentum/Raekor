#include "pch.h"
#include "VKAccelerationStructure.h"

#include "VKUtil.h"
#include "VKDevice.h"
#include "VKExtensions.h"

namespace Raekor::VK {

AccelerationStructure Device::createAccelerationStructure(VkAccelerationStructureBuildGeometryInfoKHR& buildInfo, const uint32_t primitiveCount) {
    AccelerationStructure accelStruct;
    
    // get the acceleration structure sizes
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    
    EXT::vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primitiveCount, &sizeInfo);

    accelStruct.buffer = createBuffer(
        sizeInfo.accelerationStructureSize, 
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, 
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    // create the acceleration structure buffer
    VkAccelerationStructureCreateInfoKHR asInfo = {};
    asInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    asInfo.type = buildInfo.type;
    asInfo.size = sizeInfo.accelerationStructureSize;
    asInfo.buffer = accelStruct.buffer.buffer;

    ThrowIfFailed(EXT::vkCreateAccelerationStructureKHR(device, &asInfo, nullptr, &accelStruct.accelerationStructure));

    buildInfo.dstAccelerationStructure = accelStruct.accelerationStructure;

    Buffer scratchBuffer = createBuffer(
        sizeInfo.buildScratchSize, 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    buildInfo.scratchData.deviceAddress = getDeviceAddress(scratchBuffer);

    // build the acceleration structure on the GPU
    VkCommandBuffer commandBuffer = startSingleSubmit();

    VkAccelerationStructureBuildRangeInfoKHR buildRange = {};
    buildRange.primitiveCount = primitiveCount;

    const std::array buildRanges = { &buildRange };

    EXT::vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, buildRanges.data());

    flushSingleSubmit(commandBuffer);

    destroyBuffer(scratchBuffer);

    return accelStruct;
}



void Device::destroyAccelerationStructure(AccelerationStructure& accelStruct) {
    if (accelStruct.accelerationStructure) {
        EXT::vkDestroyAccelerationStructureKHR(device, accelStruct.accelerationStructure, nullptr);
    }

    destroyBuffer(accelStruct.buffer);
}

}