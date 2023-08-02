#include "pch.h"
#include "VKPass.h"

#include "VKUtil.h"
#include "VKDevice.h"
#include "VKSwapchain.h"
#include "VKExtensions.h"

#include "Raekor/async.h"
#include "Raekor/camera.h"

namespace Raekor::VK {

void PathTracePass::Init(Device& device, const SwapChain& swapchain, const AccelStruct& accelStruct, const Buffer& instanceBuffer, const Buffer& materialBuffer, const BindlessDescriptorSet& bindlessTextures) {
    CreateRenderTargets(device, glm::uvec2(swapchain.GetExtent().width, swapchain.GetExtent().height));
    CreateDescriptorSet(device, bindlessTextures);
    UpdateDescriptorSet(device, accelStruct, instanceBuffer, materialBuffer);
    CreatePipeline(device);
    CreateShaderBindingTable(device);
}


void PathTracePass::Destroy(Device& device) {
    device.DestroyShader(m_RayGenShader);
    device.DestroyShader(m_MissShader);
    device.DestroyShader(m_ClosestHitShader);
    device.DestroyShader(m_MissShadowShader);
    device.DestroyShader(m_AnyHitShader);

    DestroyRenderTargets(device);
    vkDestroyPipeline(device, m_Pipeline, nullptr);
    vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);
    device.DestroyBuffer(m_ShaderBindingTable);
}


void PathTracePass::CreateRenderTargets(Device& device, const glm::uvec2& size) {
    Texture::Desc desc;
    desc.width = size.x;
    desc.height = size.y;
    desc.shaderAccess = true;
    desc.format = VK_FORMAT_R32G32B32A32_SFLOAT;

    finalTexture = device.CreateTexture(desc);
    accumTexture = device.CreateTexture(desc);

    device.SetDebugName(finalTexture, "finalTexture");
    device.SetDebugName(accumTexture, "accumTexture");
}



void PathTracePass::CreatePipeline(Device& device) {
    const auto vulkanSDK = getenv("VULKAN_SDK");
    assert(vulkanSDK);

    if (!FileSystem::exists("assets/system/shaders/Vulkan/bin")) {
        FileSystem::create_directory("assets/system/shaders/Vulkan/bin");
    }

    FileSystem::file_time_type timeOfMostRecentlyUpdatedIncludeFile;

    for (const auto& file : FileSystem::directory_iterator("assets/system/shaders/Vulkan/include")) {
        const auto updateTime = FileSystem::last_write_time(file);

        if (updateTime > timeOfMostRecentlyUpdatedIncludeFile) {
            timeOfMostRecentlyUpdatedIncludeFile = updateTime;
        }
    }

    for (const auto& file : FileSystem::directory_iterator("assets/system/shaders/Vulkan")) {
        if (file.is_directory()) continue;

        g_ThreadPool.QueueJob([=]() {
            auto outfile = file.path().parent_path() / "bin" / file.path().filename();
            outfile.replace_extension(outfile.extension().string() + ".spv");

            const auto textFileWriteTime = file.last_write_time();

            if (!FileSystem::exists(outfile) ||
                FileSystem::last_write_time(outfile) < textFileWriteTime ||
                timeOfMostRecentlyUpdatedIncludeFile > FileSystem::last_write_time(outfile)) {

                bool success = false;

                if (file.path().extension() == ".hlsl")
                    success = Shader::sCompileHLSL(file);
                else
                    success = Shader::sCompileGLSL(vulkanSDK, file);

                {
                    auto lock = std::scoped_lock(g_ThreadPool.GetMutex());

                    if (!success)
                        std::cout << "Compilation " << COUT_RED("failed") << " for shader: " << file.path().string() << '\n';
                    else
                        std::cout << "Compilation " << COUT_GREEN("finished") << " for shader: " << file.path().string() << '\n';
                }
            }
        });
    }

    g_ThreadPool.WaitForJobs();

    m_MissShader = device.CreateShader("assets/system/shaders/Vulkan/bin/pathtrace.rmiss.spv");
    m_RayGenShader = device.CreateShader("assets/system/shaders/Vulkan/bin/pathtrace.rgen.spv");
    m_AnyHitShader = device.CreateShader("assets/system/shaders/Vulkan/bin/pathtrace.rahit.spv");
    m_MissShadowShader = device.CreateShader("assets/system/shaders/Vulkan/bin/shadow.rmiss.spv");
    m_ClosestHitShader = device.CreateShader("assets/system/shaders/Vulkan/bin/pathtrace.rchit.spv");

    VkRayTracingShaderGroupCreateInfoKHR group = {};
    group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.intersectionShader = VK_SHADER_UNUSED_KHR;
    group.closestHitShader = VK_SHADER_UNUSED_KHR;
    group.generalShader = VK_SHADER_UNUSED_KHR;
    group.anyHitShader = VK_SHADER_UNUSED_KHR;

    // raygen
    m_ShaderGroups.emplace_back(group).generalShader = 0;
    m_ShaderGroups.back().type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;

    // miss
    m_ShaderGroups.emplace_back(group).generalShader = 1;
    m_ShaderGroups.back().type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;

    // miss shadow
    m_ShaderGroups.emplace_back(group).generalShader = 2;
    m_ShaderGroups.back().type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;

    // closest hit
    m_ShaderGroups.emplace_back(group).closestHitShader = 3;
    m_ShaderGroups.back().anyHitShader = 4;
    m_ShaderGroups.back().type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;

    auto shader_stages = std::array {
        m_RayGenShader.GetPipelineCreateInfo(),
        m_MissShader.GetPipelineCreateInfo(),
        m_MissShadowShader.GetPipelineCreateInfo(),
        m_ClosestHitShader.GetPipelineCreateInfo(),
        m_AnyHitShader.GetPipelineCreateInfo()
    };

    VkRayTracingPipelineCreateInfoKHR pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.layout = m_PipelineLayout;
    pipelineInfo.pStages = shader_stages.data();
    pipelineInfo.pGroups = m_ShaderGroups.data();
    pipelineInfo.groupCount = uint32_t(m_ShaderGroups.size());
    pipelineInfo.stageCount = uint32_t(shader_stages.size());
    pipelineInfo.maxPipelineRayRecursionDepth = 2;

    gThrowIfFailed(EXT::vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline));
}


void PathTracePass::CreateDescriptorSet(Device& device, const BindlessDescriptorSet& bindlessTextures) {
    VkDescriptorSetLayoutBinding binding0 = {};
    binding0.binding = 0;
    binding0.descriptorCount = 1;
    binding0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    binding0.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding binding1 = {};
    binding1.binding = 1;
    binding1.descriptorCount = 1;
    binding1.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    binding1.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding binding2 = {};
    binding2.binding = 2;
    binding2.descriptorCount = 1;
    binding2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding2.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding binding3 = {};
    binding3.binding = 3;
    binding3.descriptorCount = 1;
    binding3.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding3.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding binding4 = {};
    binding4.binding = 4;
    binding4.descriptorCount = 1;
    binding4.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    binding4.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    const auto bindings = std::array { binding0, binding1, binding2, binding3, binding4 };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {};
    descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo.bindingCount = uint32_t(bindings.size());
    descriptorSetLayoutInfo.pBindings = bindings.data();

    gThrowIfFailed(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutInfo, nullptr, &m_DescriptorSetLayout));

    device.AllocateDescriptorSet(1, &m_DescriptorSetLayout, &m_DescriptorSet);

    VkPushConstantRange push_constant_range = {};
    push_constant_range.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                     VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                                     VK_SHADER_STAGE_MISS_BIT_KHR |
                                     VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    push_constant_range.size = sizeof(PushConstants); // TODO: query size from physical device

    const auto layouts = std::array { m_DescriptorSetLayout, bindlessTextures.GetLayout() };

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pSetLayouts = layouts.data();
    layoutInfo.setLayoutCount = uint32_t(layouts.size());
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &push_constant_range;

    gThrowIfFailed(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_PipelineLayout));
}



void PathTracePass::UpdateDescriptorSet(Device& device, const AccelStruct& accelStruct, const Buffer& instanceBuffer, const Buffer& materialBuffer) {
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.imageView = device.CreateView(finalTexture);

    VkWriteDescriptorSet write0 = {};
    write0.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write0.descriptorCount = 1;
    write0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write0.dstBinding = 0;
    write0.dstSet = m_DescriptorSet;
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
    write1.dstSet = m_DescriptorSet;
    write1.pNext = &accelStructInfo;

    VkDescriptorBufferInfo bufferInfo2 = {};
    bufferInfo2.range = VK_WHOLE_SIZE;
    bufferInfo2.buffer = instanceBuffer.buffer;

    VkWriteDescriptorSet write2 = {};
    write2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write2.descriptorCount = 1;
    write2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write2.dstBinding = 2;
    write2.dstSet = m_DescriptorSet;
    write2.pBufferInfo = &bufferInfo2;

    VkDescriptorBufferInfo bufferInfo3 = {};
    bufferInfo3.range = VK_WHOLE_SIZE;
    bufferInfo3.buffer = materialBuffer.buffer;

    VkWriteDescriptorSet write3 = {};
    write3.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write3.descriptorCount = 1;
    write3.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write3.dstBinding = 3;
    write3.dstSet = m_DescriptorSet;
    write3.pBufferInfo = &bufferInfo3;

    VkDescriptorImageInfo imageInfo2 = {};
    imageInfo2.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo2.imageView = device.CreateView(accumTexture);

    VkWriteDescriptorSet write4 = {};
    write4.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write4.descriptorCount = 1;
    write4.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write4.dstBinding = 4;
    write4.dstSet = m_DescriptorSet;
    write4.pImageInfo = &imageInfo2;

    std::array writes = { write0, write1, write2, write3, write4 };

    vkUpdateDescriptorSets(device, uint32_t(writes.size()), writes.data(), 0, nullptr);
}



void PathTracePass::CreateShaderBindingTable(Device& device) {
    const auto& ray_tracing_properties = device.GetPhysicalProperties().rayTracingPipelineProperties;

    const auto group_count = uint32_t(m_ShaderGroups.size());

    const auto aligned_group_size = uint32_t(gAlignUp(
                                          ray_tracing_properties.shaderGroupHandleSize,
                                          ray_tracing_properties.shaderGroupBaseAlignment
                                      ));

    const auto table_size = group_count * aligned_group_size;

    m_ShaderBindingTable = device.CreateBuffer(
                table_size,
                VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VMA_MEMORY_USAGE_CPU_ONLY
            );

    auto shader_handle_storage = std::vector<uint8_t>(table_size);

    gThrowIfFailed(EXT::vkGetRayTracingShaderGroupHandlesKHR(device, m_Pipeline, 0, group_count, table_size, shader_handle_storage.data()));

    auto mapped_ptr = device.GetMappedPointer<uint8_t*>(m_ShaderBindingTable);

    for (uint32_t group = 0; group < group_count; group++) {
        const auto data_ptr = shader_handle_storage.data() + group * ray_tracing_properties.shaderGroupHandleSize;
        memcpy(mapped_ptr, data_ptr, ray_tracing_properties.shaderGroupHandleSize);
        mapped_ptr += aligned_group_size;
    }
}



void PathTracePass::Record(const Device& device, const Viewport& viewport, VkCommandBuffer commandBuffer, const BindlessDescriptorSet& bindlessTextures) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_Pipeline);

    std::array descriptor_sets = {
        /* set = 0 */ m_DescriptorSet,
        /* set = 1 */ bindlessTextures.GetDescriptorSet()
    };

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_PipelineLayout,
                            0, uint32_t(descriptor_sets.size()), descriptor_sets.data(),
                            0, nullptr
                           );

    m_PushConstants.invViewProj = glm::inverse(viewport.GetCamera().GetProjection() * viewport.GetCamera().GetView());
    m_PushConstants.cameraPosition = glm::vec4(viewport.GetCamera().GetPosition(), 1.0);

    constexpr VkShaderStageFlags stages = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                          VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                                          VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                                          VK_SHADER_STAGE_MISS_BIT_KHR;

    vkCmdPushConstants(commandBuffer, m_PipelineLayout, stages, 0, sizeof(m_PushConstants), &m_PushConstants);

    m_PushConstants.frameCounter++;

    const auto& ray_tracing_properties = device.GetPhysicalProperties().rayTracingPipelineProperties;

    const uint32_t aligned_group_size = uint32_t(gAlignUp(
                                          ray_tracing_properties.shaderGroupHandleSize,
                                          ray_tracing_properties.shaderGroupBaseAlignment
                                      ));

    const auto table_buffer_device_address = device.GetDeviceAddress(m_ShaderBindingTable);

    VkStridedDeviceAddressRegionKHR raygen_region = {};
    raygen_region.deviceAddress = table_buffer_device_address;
    raygen_region.size = aligned_group_size;
    raygen_region.stride = aligned_group_size;

    VkStridedDeviceAddressRegionKHR miss_region = {};
    miss_region.deviceAddress = raygen_region.deviceAddress + raygen_region.size;
    miss_region.size = aligned_group_size * 2;
    miss_region.stride = aligned_group_size;

    VkStridedDeviceAddressRegionKHR hit_region = {};
    hit_region.deviceAddress = miss_region.deviceAddress + miss_region.size;
    hit_region.size = aligned_group_size;
    hit_region.stride = aligned_group_size;

    VkStridedDeviceAddressRegionKHR callable_region = {};

    EXT::vkCmdTraceRaysKHR(commandBuffer, &raygen_region, &miss_region, &hit_region, &callable_region, viewport.GetSize().x, viewport.GetSize().y, 1);
}


void PathTracePass::ReloadShaders(Device& device) {
    vkQueueWaitIdle(device.GetQueue());

    vkDestroyPipeline(device, m_Pipeline, nullptr);

    device.DestroyShader(m_RayGenShader);
    device.DestroyShader(m_MissShader);
    device.DestroyShader(m_ClosestHitShader);
    device.DestroyShader(m_MissShadowShader);
    device.DestroyShader(m_AnyHitShader);
    
    CreatePipeline(device);
}


void PathTracePass::DestroyRenderTargets(Device& device) {
    device.DestroyTexture(finalTexture);
    device.DestroyTexture(accumTexture);
}

}