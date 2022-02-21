#include "pch.h"
#include "VKStructs.h"

#include "VKUtil.h"
#include "VKDevice.h"
#include "VKExtensions.h"

namespace Raekor::VK {

BVH Device::CreateBVH(VkAccelerationStructureBuildGeometryInfoKHR& buildInfo, const uint32_t primitiveCount) {
    BVH bvh;
    
    // get the acceleration structure sizes
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    
    EXT::vkGetAccelerationStructureBuildSizesKHR(m_Device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primitiveCount, &sizeInfo);

    bvh.buffer = CreateBuffer(
        sizeInfo.accelerationStructureSize, 
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, 
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    // create the acceleration structure buffer
    VkAccelerationStructureCreateInfoKHR asInfo = {};
    asInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    asInfo.type = buildInfo.type;
    asInfo.size = sizeInfo.accelerationStructureSize;
    asInfo.buffer = bvh.buffer.buffer;

    gThrowIfFailed(EXT::vkCreateAccelerationStructureKHR(m_Device, &asInfo, nullptr, &bvh.accelerationStructure));

    buildInfo.dstAccelerationStructure = bvh.accelerationStructure;

    Buffer scratchBuffer = CreateBuffer(
        sizeInfo.buildScratchSize, 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    buildInfo.scratchData.deviceAddress = GetDeviceAddress(scratchBuffer);

    // build the acceleration structure on the GPU
    VkCommandBuffer commandBuffer = StartSingleSubmit();

    VkAccelerationStructureBuildRangeInfoKHR buildRange = {};
    buildRange.primitiveCount = primitiveCount;

    const std::array buildRanges = { &buildRange };

    EXT::vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, buildRanges.data());

    FlushSingleSubmit(commandBuffer);

    DestroyBuffer(scratchBuffer);

    return bvh;
}


void Device::DestroyBVH(BVH& accelStruct) {
    if (accelStruct.accelerationStructure) {
        EXT::vkDestroyAccelerationStructureKHR(m_Device, accelStruct.accelerationStructure, nullptr);
    }

    DestroyBuffer(accelStruct.buffer);
}

}