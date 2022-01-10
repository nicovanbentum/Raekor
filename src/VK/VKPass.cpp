#include "pch.h"
#include "VKPass.h"

#include "VKUtil.h"
#include "VKDevice.h"
#include "VKSwapchain.h"
#include "VKExtensions.h"
#include "VKAccelerationStructure.h"

#include "camera.h"
#include "async.h"

namespace Raekor::VK {

void PathTracePass::initialize(Device& device, const Swapchain& swapchain, const AccelerationStructure& accelStruct, const Buffer& instanceBuffer, const Buffer& materialBuffer, const BindlessDescriptorSet& bindlessTextures) {
    createRenderTextures(device, glm::uvec2(swapchain.getExtent().width, swapchain.getExtent().height));
    createDescriptorSet(device, bindlessTextures);
    updateDescriptorSet(device, accelStruct, instanceBuffer, materialBuffer);
    createPipeline(device);
    createShaderBindingTable(device);
}

void PathTracePass::destroy(Device& device) {
    device.destroyShader(rgenShader);
    device.destroyShader(rmissShader);
    device.destroyShader(rchitShader);
    device.destroyShader(rmissShadowShader);

    destroyRenderTextures(device);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    device.destroyBuffer(shaderBindingTable);
}


void PathTracePass::createRenderTextures(Device& device, const glm::uvec2& size) {
    {
        Texture::Desc desc;
        desc.width = size.x;
        desc.height = size.y;
        desc.shaderAccess = true;
        desc.format = VK_FORMAT_R32G32B32A32_SFLOAT;

        finalTexture = device.createTexture(desc);
        accumTexture = device.createTexture(desc);

        device.setDebugName(finalTexture, "finalTexture");
        device.setDebugName(accumTexture, "accumTexture");
    }
}



void PathTracePass::createPipeline(Device& device) {
    const auto vulkanSDK = getenv("VULKAN_SDK");
    assert(vulkanSDK);

    if (!fs::exists("shaders/Vulkan/bin")) {
        fs::create_directory("shaders/Vulkan/bin");
    }

    fs::file_time_type timeOfMostRecentlyUpdatedIncludeFile;

    for (const auto& file : fs::directory_iterator("shaders/Vulkan/include")) {
        const auto updateTime = fs::last_write_time(file);

        if (updateTime > timeOfMostRecentlyUpdatedIncludeFile) {
            timeOfMostRecentlyUpdatedIncludeFile = updateTime;
        }
    }

    for (const auto& file : fs::directory_iterator("shaders/Vulkan")) {
        if (file.is_directory()) continue;

        Async::dispatch([=]() {
            auto outfile = file.path().parent_path() / "bin" / file.path().filename();
            outfile.replace_extension(outfile.extension().string() + ".spv");

            const auto textFileWriteTime = file.last_write_time();

            if (!fs::exists(outfile) ||
                fs::last_write_time(outfile) < textFileWriteTime ||
                timeOfMostRecentlyUpdatedIncludeFile > fs::last_write_time(outfile)) {

                bool success = false;

                if (file.path().extension() == ".hlsl") {
                    success = Shader::DXC(file);
                } else {
                    success = Shader::glslangValidator(vulkanSDK, file);
                }

                {
                    auto lock = Async::lock();

                    if (!success) {
                        std::cout << "Compilation " << COUT_RED("failed") << " for shader: " << file.path().string() << '\n';
                    } else {
                        std::cout << "Compilation " << COUT_GREEN("finished") << " for shader: " << file.path().string() << '\n';
                    }
                }
            }
        });
    }

    Async::wait();

    rgenShader = device.createShader("shaders/Vulkan/bin/pathtrace.rgen.spv");
    rmissShader = device.createShader("shaders/Vulkan/bin/pathtrace.rmiss.spv");
    rchitShader = device.createShader("shaders/Vulkan/bin/pathtrace.rchit.spv");
    rmissShadowShader = device.createShader("shaders/Vulkan/bin/shadow.rmiss.spv");

    std::array shaderStages = {
        rgenShader.getPipelineCreateInfo(),
        rmissShader.getPipelineCreateInfo(),
        rmissShadowShader.getPipelineCreateInfo(),
        rchitShader.getPipelineCreateInfo()
    };

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

    // miss shadow
    shaderGroups.emplace_back(group).generalShader = 2;
    shaderGroups.back().type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;

    // closest hit
    shaderGroups.emplace_back(group).closestHitShader = 3;
    shaderGroups.back().type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;

    VkRayTracingPipelineCreateInfoKHR pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pGroups = shaderGroups.data();
    pipelineInfo.groupCount = uint32_t(shaderGroups.size());
    pipelineInfo.stageCount = uint32_t(shaderStages.size());
    pipelineInfo.maxPipelineRayRecursionDepth = 1;

    ThrowIfFailed(EXT::vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
}



void PathTracePass::createDescriptorSet(Device& device, const BindlessDescriptorSet& bindlessTextures) {
    VkDescriptorSetLayoutBinding binding0 = {};
    binding0.binding = 0;
    binding0.descriptorCount = 1;
    binding0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    binding0.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding binding1 = {};
    binding1.binding = 1;
    binding1.descriptorCount = 1;
    binding1.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    binding1.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding binding2 = {};
    binding2.binding = 2;
    binding2.descriptorCount = 1;
    binding2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding2.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding binding3 = {};
    binding3.binding = 3;
    binding3.descriptorCount = 1;
    binding3.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding3.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding binding4 = {};
    binding4.binding = 4;
    binding4.descriptorCount = 1;
    binding4.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    binding4.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    std::array bindings = { binding0, binding1, binding2, binding3, binding4 };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {};
    descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo.bindingCount = uint32_t(bindings.size());
    descriptorSetLayoutInfo.pBindings = bindings.data();

    ThrowIfFailed(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout));

    device.allocateDescriptorSet(1, &descriptorSetLayout, &descriptorSet);

    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                                   VK_SHADER_STAGE_MISS_BIT_KHR;
    pushConstantRange.size = sizeof(PushConstants); // TODO: query size from physical device

    std::array layouts = { descriptorSetLayout, bindlessTextures.getLayout() };

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pSetLayouts = layouts.data();
    layoutInfo.setLayoutCount = uint32_t(layouts.size());
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    ThrowIfFailed(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout));
}



void PathTracePass::updateDescriptorSet(Device& device, const AccelerationStructure& accelStruct, const Buffer& instanceBuffer, const Buffer& materialBuffer) {
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.imageView = device.createView(finalTexture);

    VkWriteDescriptorSet write0 = {};
    write0.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write0.descriptorCount = 1;
    write0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write0.dstBinding = 0;
    write0.dstSet = descriptorSet;
    write0.pImageInfo = &imageInfo;

    VkWriteDescriptorSetAccelerationStructureKHR accelStructInfo = {};
    accelStructInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    accelStructInfo.pAccelerationStructures = &accelStruct.accelerationStructure;
    accelStructInfo.accelerationStructureCount = 1;

    VkWriteDescriptorSet write1 = {};
    write1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write1.descriptorCount = 1;
    write1.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    write1.dstBinding = 1;
    write1.dstSet = descriptorSet;
    write1.pNext = &accelStructInfo;

    VkDescriptorBufferInfo bufferInfo2 = {};
    bufferInfo2.range = VK_WHOLE_SIZE;
    bufferInfo2.buffer = instanceBuffer.buffer;

    VkWriteDescriptorSet write2 = {};
    write2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write2.descriptorCount = 1;
    write2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write2.dstBinding = 2;
    write2.dstSet = descriptorSet;
    write2.pBufferInfo = &bufferInfo2;

    VkDescriptorBufferInfo bufferInfo3 = {};
    bufferInfo3.range = VK_WHOLE_SIZE;
    bufferInfo3.buffer = materialBuffer.buffer;

    VkWriteDescriptorSet write3 = {};
    write3.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write3.descriptorCount = 1;
    write3.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write3.dstBinding = 3;
    write3.dstSet = descriptorSet;
    write3.pBufferInfo = &bufferInfo3;

    VkDescriptorImageInfo imageInfo2 = {};
    imageInfo2.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo2.imageView = device.createView(accumTexture);

    VkWriteDescriptorSet write4 = {};
    write4.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write4.descriptorCount = 1;
    write4.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write4.dstBinding = 4;
    write4.dstSet = descriptorSet;
    write4.pImageInfo = &imageInfo2;

    std::array writes = { write0, write1, write2, write3, write4 };

    vkUpdateDescriptorSets(device, uint32_t(writes.size()), writes.data(), 0, nullptr);
}



void PathTracePass::createShaderBindingTable(Device& device) {
    const auto& rayTracingProperties = device.getPhysicalProperties().rayTracingPipelineProperties;

    const uint32_t groupCount = uint32_t(shaderGroups.size());

    const uint32_t alignedGroupSize = uint32_t(align_up(
                                          rayTracingProperties.shaderGroupHandleSize,
                                          rayTracingProperties.shaderGroupBaseAlignment
                                      ));

    const uint32_t sbtSize = groupCount * alignedGroupSize;

    shaderBindingTable = device.createBuffer(
                sbtSize,
                VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VMA_MEMORY_USAGE_CPU_ONLY
            );

    std::vector<uint8_t> shaderHandleStorage(sbtSize);

    ThrowIfFailed(EXT::vkGetRayTracingShaderGroupHandlesKHR(device, pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()));

    auto mappedPtr = static_cast<uint8_t*>(device.getMappedPointer(shaderBindingTable));

    for (uint32_t group = 0; group < groupCount; group++) {
        const auto dataPtr = shaderHandleStorage.data() + group * rayTracingProperties.shaderGroupHandleSize;
        memcpy(mappedPtr, dataPtr, rayTracingProperties.shaderGroupHandleSize);
        mappedPtr += alignedGroupSize;
    }
}



void PathTracePass::recordCommands(const Device& device, const Viewport& viewport, VkCommandBuffer commandBuffer, const BindlessDescriptorSet& bindlessTextures) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);

    std::array descriptorSets = {
        /* set = 0 */ descriptorSet,
        /* set = 1 */ bindlessTextures.getDescriptorSet()
    };

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout,
                            0, uint32_t(descriptorSets.size()), descriptorSets.data(),
                            0, nullptr
                           );

    pushConstants.invViewProj = glm::inverse(viewport.getCamera().getProjection() * viewport.getCamera().getView());
    pushConstants.cameraPosition = glm::vec4(viewport.getCamera().getPosition(), 1.0);

    constexpr VkShaderStageFlags stages = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                          VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                                          VK_SHADER_STAGE_MISS_BIT_KHR;

    vkCmdPushConstants(commandBuffer, pipelineLayout, stages, 0, sizeof(pushConstants), &pushConstants);

    pushConstants.frameCounter++;

    const auto& rayTracingProperties = device.getPhysicalProperties().rayTracingPipelineProperties;

    const uint32_t alignedGroupSize = uint32_t(align_up(
                                          rayTracingProperties.shaderGroupHandleSize,
                                          rayTracingProperties.shaderGroupBaseAlignment
                                      ));

    const auto SBTBufferDeviceAddress = device.getDeviceAddress(shaderBindingTable);

    VkStridedDeviceAddressRegionKHR raygenRegion = {};
    raygenRegion.deviceAddress = SBTBufferDeviceAddress;
    raygenRegion.size = alignedGroupSize;
    raygenRegion.stride = alignedGroupSize;

    VkStridedDeviceAddressRegionKHR missRegion = {};
    missRegion.deviceAddress = raygenRegion.deviceAddress + raygenRegion.size;
    missRegion.size = alignedGroupSize * 2;
    missRegion.stride = alignedGroupSize;

    VkStridedDeviceAddressRegionKHR hitRegion = {};
    hitRegion.deviceAddress = missRegion.deviceAddress + missRegion.size;
    hitRegion.size = alignedGroupSize;
    hitRegion.stride = alignedGroupSize;

    VkStridedDeviceAddressRegionKHR callableRegion = {};

    EXT::vkCmdTraceRaysKHR(commandBuffer, &raygenRegion, &missRegion, &hitRegion, &callableRegion, viewport.size.x, viewport.size.y, 1);
}



void PathTracePass::reloadShadersFromDisk(Device& device) {
    vkQueueWaitIdle(device.queue);

    vkDestroyPipeline(device, pipeline, nullptr);

    device.destroyShader(rgenShader);
    device.destroyShader(rmissShader);
    device.destroyShader(rchitShader);
    device.destroyShader(rmissShadowShader);
    
    createPipeline(device);
}



void PathTracePass::destroyRenderTextures(Device& device) {
    device.destroyTexture(finalTexture);
    device.destroyTexture(accumTexture);
}

}