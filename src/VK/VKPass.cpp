#include "pch.h"
#include "VKPass.h"
#include "VKExtensions.h"

namespace Raekor::VK {

void PathTracePass::initialize(Context& context) {
    Shader::compileFromCommandLine("shaders/Vulkan/pathtrace.rgen", "shaders/Vulkan/bin/pathtrace.rgen.spv");
    Shader::compileFromCommandLine("shaders/Vulkan/pathtrace.rmiss", "shaders/Vulkan/bin/pathtrace.rmiss.spv");
    Shader::compileFromCommandLine("shaders/Vulkan/pathtrace.rchit", "shaders/Vulkan/bin/pathtrace.rchit.spv");
    rgenShader.create(context.device, "shaders/Vulkan/bin/pathtrace.rgen.spv");
    rmissShader.create(context.device, "shaders/Vulkan/bin/pathtrace.rmiss.spv");
    rchitShader.create(context.device, "shaders/Vulkan/bin/pathtrace.rchit.spv");

    createPipeline(context.device);
    createShaderBindingTable(context.device, context.physicalDevice);
}

void PathTracePass::destroy(Device& device) {
    rgenShader.destroy(device);
    rmissShader.destroy(device);
    rchitShader.destroy(device);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vmaDestroyBuffer(device.getAllocator(), shaderBindingTableBuffer, shaderBindingTableAllocation);
}

void PathTracePass::createPipeline(Device& device) {
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {};
    descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    assert(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout) == VK_SUCCESS);

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pSetLayouts = &descriptorSetLayout;

    assert(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS);

    VkPipelineShaderStageCreateInfo raygenShaderInfo = {};
    raygenShaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    raygenShaderInfo.pName = "main";
    raygenShaderInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    raygenShaderInfo.module = rgenShader.getModule();

    VkPipelineShaderStageCreateInfo rmissShaderInfo = {};
    rmissShaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    rmissShaderInfo.pName = "main";
    rmissShaderInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    rmissShaderInfo.module = rmissShader.getModule();

    VkPipelineShaderStageCreateInfo rchitShaderInfo = {};
    rchitShaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    rchitShaderInfo.pName = "main";
    rchitShaderInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    rchitShaderInfo.module = rchitShader.getModule();

    std::array shaderStages = { raygenShaderInfo, rmissShaderInfo, rchitShaderInfo };

    VkRayTracingShaderGroupCreateInfoKHR group = {};
    group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.intersectionShader = VK_SHADER_UNUSED_KHR;
    group.closestHitShader = VK_SHADER_UNUSED_KHR;
    group.generalShader = VK_SHADER_UNUSED_KHR;
    group.anyHitShader = VK_SHADER_UNUSED_KHR;

    // raygen
    shaderGroups.emplace_back(group).generalShader = 0;
    shaderGroups.back().type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;

    // miss
    shaderGroups.emplace_back(group).generalShader = 1;
    shaderGroups.back().type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;

    // closest hit
    shaderGroups.emplace_back(group).closestHitShader = 2;
    shaderGroups.back().type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;

    VkRayTracingPipelineCreateInfoKHR pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pGroups = shaderGroups.data();
    pipelineInfo.groupCount = static_cast<uint32_t>(shaderGroups.size());
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.maxPipelineRayRecursionDepth = 1;

    assert(EXT::vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) == VK_SUCCESS);
}

void PathTracePass::createShaderBindingTable(Device& device, PhysicalDevice& physicalDevice) {
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties = {};
    rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    
    VkPhysicalDeviceProperties2 properties = {};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &rayTracingProperties;
    
    vkGetPhysicalDeviceProperties2(physicalDevice, &properties);

    const uint32_t groupCount = static_cast<uint32_t>(shaderGroups.size());
    const uint32_t sbtSize = groupCount * rayTracingProperties.shaderGroupBaseAlignment;

    auto [buffer, allocation] = device.createBuffer(sbtSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR, VMA_MEMORY_USAGE_CPU_ONLY, true);
    shaderBindingTableBuffer = buffer;
    shaderBindingTableAllocation = allocation;

    std::vector<uint8_t> shaderHandleStorage(sbtSize);

    if (EXT::vkGetRayTracingShaderGroupHandlesKHR(device, pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to retrieve ray tracing shader group handles");
    }

    auto mappedPtr = static_cast<uint8_t*>(device.getMappedPointer(allocation));

    for (uint32_t group = 0; group < groupCount; group++) {
        memcpy(
            mappedPtr + group * rayTracingProperties.shaderGroupBaseAlignment,
            shaderHandleStorage.data() + group * rayTracingProperties.shaderGroupHandleSize,
            rayTracingProperties.shaderGroupHandleSize
        );
    }

}

}