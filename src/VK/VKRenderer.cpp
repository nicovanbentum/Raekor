#include "pch.h"
#include "VKRenderer.h"

#include "VKUtil.h"
#include "VKDevice.h"
#include "VKExtensions.h"
#include "VKAccelerationStructure.h"

namespace Raekor::VK {

Renderer::Renderer(SDL_Window* window) : 
    device(window) 
{
    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    VkPresentModeKHR mode = vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;

    swapchain.create(device, { w, h }, mode);

    setupSyncObjects();

    bindlessTextures.create(device, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
}



Renderer::~Renderer() {
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    vkDeviceWaitIdle(device);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, imageAcquiredSemaphores[i], nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(device, commandsFinishedFences[i], nullptr);
    }

    for (auto& texture : textures) {
        device.destroyTexture(texture);
    }

    for (auto& sampler : samplers) {
        device.destroySampler(sampler);
    }


    TLAS.destroy(device);
    swapchain.destroy(device);
    imGuiPass.destroy(device);
    pathTracePass.destroy(device);
    bindlessTextures.destroy(device);
    device.destroyBuffer(instanceBuffer);
    device.destroyBuffer(materialBuffer);
}



void Renderer::initialize(Scene& scene) {
    pathTracePass.initialize(device, swapchain, TLAS, instanceBuffer, materialBuffer, bindlessTextures);
    imGuiPass.initialize(device, swapchain, pathTracePass, bindlessTextures);
}



void Renderer::resetAccumulation() {
    pathTracePass.pushConstants.frameCounter = 0;
}



void Renderer::updateMaterials(Assets& assets, Scene& scene) {
    auto view = scene.view<Material>();

    std::vector<RTMaterial> materials(view.size());

    auto raw = *view.raw();

    for (int i = 0; i < view.size(); i++) {
        auto& material = raw[i];

        auto& buffer = materials[i];
        buffer.albedo = material.baseColour;

        buffer.textures.x = addBindlessTexture(device, assets.get<TextureAsset>(material.albedoFile), VK_FORMAT_BC3_SRGB_BLOCK);
        buffer.textures.y = addBindlessTexture(device, assets.get<TextureAsset>(material.normalFile), VK_FORMAT_BC3_UNORM_BLOCK);
        buffer.textures.z = addBindlessTexture(device, assets.get<TextureAsset>(material.mrFile), VK_FORMAT_BC3_UNORM_BLOCK);

        buffer.properties.x = material.metallic;
        buffer.properties.y = material.roughness;
        buffer.properties.z = 1.0f;
    }

    const auto materialBufferSize = materials.size() * sizeof(materials[0]);

    materialBuffer = device.createBuffer(
        materialBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    auto mappedPtr = device.getMappedPointer(materialBuffer);
    memcpy(mappedPtr, materials.data(), materialBufferSize);
}

void Renderer::updateAccelerationStructures(Scene& scene) {
    auto meshes = scene.view<Mesh, Transform, VK::RTGeometry>();

    if (meshes.size_hint() < 1) return;

    std::vector<VkAccelerationStructureInstanceKHR> deviceInstances;
    deviceInstances.reserve(meshes.size_hint());

    uint32_t customIndex = 0; 
    for (auto& [entity, mesh, transform, geometry] : meshes.each()) {
        auto deviceAddress = device.getDeviceAddress(geometry.accelStruct.accelerationStructure);

        VkAccelerationStructureInstanceKHR instance = {};
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.mask = 0xFF;
        instance.instanceCustomIndex = customIndex++;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.accelerationStructureReference = deviceAddress;
        instance.transform = VkTransformMatrix(transform.worldTransform);

        deviceInstances.push_back(instance);
    }

    TLAS.destroy(device);
    TLAS = createTLAS(deviceInstances.data(), deviceInstances.size());

    struct Instance {
        glm::ivec4 materialIndex;
        glm::mat4 localToWorldTransform;
        VkDeviceAddress indexBufferAddress;
        VkDeviceAddress vertexBufferAddress;
    };

    std::vector<Instance> hostInstances;
    hostInstances.reserve(meshes.size_hint());

    auto materials = scene.view<Material>();

    for (auto& [entity, mesh, transform, geometry] : meshes.each()) {
        Instance instance = {};
        instance.materialIndex.x = int32_t(materials.raw_index(mesh.material));
        instance.localToWorldTransform = transform.worldTransform;
        instance.indexBufferAddress = device.getDeviceAddress(geometry.indices);
        instance.vertexBufferAddress = device.getDeviceAddress(geometry.vertices);

        hostInstances.push_back(instance);
    }

    instanceBuffer = device.createBuffer(
        sizeof(Instance) * meshes.size_hint(), 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    auto mappedPtr = device.getMappedPointer(instanceBuffer);

    memcpy(mappedPtr, hostInstances.data(), hostInstances.size() * sizeof(Instance));
}



void Renderer::render(SDL_Window* window, const Viewport& viewport, Scene& scene) {
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    vkWaitForFences(device, 1, &commandsFinishedFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &commandsFinishedFences[currentFrame]);

    VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAcquiredSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain(window);
        return;
    }

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    auto commandBuffer = swapchain.submitBuffers[currentFrame];

    ThrowIfFailed(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    {
        VkImageMemoryBarrier2KHR barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR | 
                                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        VkDependencyInfoKHR dep = {};
        dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers = &barrier;

        barrier.image = pathTracePass.finalTexture.image;
        EXT::vkCmdPipelineBarrier2KHR(commandBuffer, &dep);

        barrier.image = pathTracePass.accumTexture.image;
        EXT::vkCmdPipelineBarrier2KHR(commandBuffer, &dep);
    }

    pathTracePass.recordCommands(device, viewport, commandBuffer, bindlessTextures);
    
    imGuiPass.record(device, commandBuffer, ImGui::GetDrawData(), bindlessTextures, viewport.size.x, viewport.size.y, pathTracePass);

    {
        VkImageMemoryBarrier2KHR barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.image = swapchain.images[imageIndex];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        VkDependencyInfoKHR dep = {};
        dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers = &barrier;

        EXT::vkCmdPipelineBarrier2KHR(commandBuffer, &dep);
    }

    VkImageBlit region = {};

    region.srcOffsets[1] = { int32_t(viewport.size.x), int32_t(viewport.size.y), 1 };
    region.srcSubresource.layerCount = 1;
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    region.dstOffsets[1] = { int32_t(viewport.size.x), int32_t(viewport.size.y), 1 };
    region.dstSubresource.layerCount = 1;
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    vkCmdBlitImage(commandBuffer, pathTracePass.finalTexture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchain.images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST);

    {
        VkImageMemoryBarrier2KHR barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR;
        barrier.image = swapchain.images[imageIndex];
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        VkDependencyInfoKHR dep = {};
        dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers = &barrier;

        EXT::vkCmdPipelineBarrier2KHR(commandBuffer, &dep);
    }

    ThrowIfFailed(vkEndCommandBuffer(commandBuffer));

    VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR; // For now

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAcquiredSemaphores[currentFrame];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinishedSemaphores[imageIndex];
    submitInfo.pWaitDstStageMask = &waitStages;

    ThrowIfFailed(vkQueueSubmit(device.queue, 1, &submitInfo, commandsFinishedFences[currentFrame]));

    VkSwapchainKHR swapChains[] = { swapchain };

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphores[imageIndex];

    result = vkQueuePresentKHR(device.queue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain(window);
    }  else {
        ThrowIfFailed(result);
    }
}



void Renderer::setupSyncObjects() {
    imageAcquiredSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    commandsFinishedFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAcquiredSemaphores[i]);
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]);
        vkCreateFence(device, &fenceInfo, nullptr, &commandsFinishedFences[i]);
    }
}



void Renderer::recreateSwapchain(SDL_Window* window) {
    vkQueueWaitIdle(device.queue);
    
    uint32_t flags = SDL_GetWindowFlags(window);
    while (flags & SDL_WINDOW_MINIMIZED) {
        flags = SDL_GetWindowFlags(window);
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {}
    }

    VkPresentModeKHR mode = vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
    
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    
    swapchain.destroy(device);
    swapchain.create(device, { w, h }, mode);

    pathTracePass.destroyRenderTextures(device);
    pathTracePass.createRenderTextures(device, glm::uvec2(w, h));
    pathTracePass.updateDescriptorSet(device, TLAS, instanceBuffer, materialBuffer);

    imGuiPass.destroyFramebuffer(device);
    imGuiPass.createFramebuffer(device, pathTracePass, uint32_t(w), uint32_t(h));
}

int32_t Renderer::addBindlessTexture(Device& device, const std::shared_ptr<TextureAsset>& asset, VkFormat format) {
    if (!asset) {
        return -1;
    }

    Texture::Desc desc;
    desc.format = format;
    desc.width = asset->header()->dwWidth;
    desc.height = asset->header()->dwHeight;
    desc.mipLevels = asset->header()->dwMipMapCount;

    auto texture = device.createTexture(desc);
    auto view = device.createView(texture);

    Sampler::Desc samplerDesc;
    samplerDesc.minFilter = VK_FILTER_LINEAR;
    samplerDesc.magFilter = VK_FILTER_LINEAR;
    samplerDesc.anisotropy = 16.0f;
    samplerDesc.maxMipmap = float(desc.mipLevels);

    auto sampler = samplers.emplace_back(device.createSampler(samplerDesc));

    auto buffer = device.createBuffer(
        align_up(asset->getDataSize(), 16), // the disk size is not aligned to 16
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VMA_MEMORY_USAGE_CPU_ONLY
    );

    auto mappedPtr = device.getMappedPointer(buffer);

    memcpy(mappedPtr, asset->getData(), asset->getDataSize());

    device.transitionImageLayout(
        texture,
        VK_IMAGE_LAYOUT_UNDEFINED, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    std::vector<VkBufferImageCopy> regions(desc.mipLevels);

    uint32_t mip = 0, bufferOffset = 0;

    for (auto& region : regions) {
        glm::uvec2 mipDimensions = { 
            std::max(desc.width >> mip, 1u), 
            std::max(desc.height >> mip, 1u) 
        };

        region.bufferOffset = bufferOffset;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = mip++;
        region.imageSubresource.layerCount = 1;
        
        region.imageExtent = { mipDimensions.x, mipDimensions.y, 1 };
        
        bufferOffset +=  mipDimensions.x * mipDimensions.y;
    }

    auto commandBuffer = device.startSingleSubmit();

    vkCmdCopyBufferToImage(
        commandBuffer, buffer.buffer, texture.image, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
        uint32_t(regions.size()), regions.data()
    );

    device.flushSingleSubmit(commandBuffer);

    device.transitionImageLayout(
        texture,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    device.destroyBuffer(buffer);
    textures.push_back(texture);

    VkDescriptorImageInfo descriptorInfo = {};
    descriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptorInfo.imageView = view;
    descriptorInfo.sampler = sampler.native();

    return bindlessTextures.append(device, descriptorInfo);
}



void Renderer::reloadShaders() {
    pathTracePass.reloadShadersFromDisk(device);
}



void Renderer::setVsync(bool on) { 
    vsync = on; 
}



RTGeometry Renderer::createBLAS(Mesh& mesh) {
    auto vertices = mesh.getInterleavedVertices();
    const auto sizeOfVertexBuffer = vertices.size() * sizeof(vertices[0]);
    const auto sizeOfIndexBuffer = mesh.indices.size() * sizeof(mesh.indices[0]);

    auto vertexStageBuffer = device.createBuffer(sizeOfVertexBuffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    auto indexStageBuffer = device.createBuffer(sizeOfIndexBuffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    memcpy(device.getMappedPointer(vertexStageBuffer), vertices.data(), sizeOfVertexBuffer);
    memcpy(device.getMappedPointer(indexStageBuffer), mesh.indices.data(), sizeOfIndexBuffer);

    constexpr auto bufferUsages = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | 
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    
    RTGeometry component = {};

    component.vertices = device.createBuffer(
        sizeOfVertexBuffer, 
        bufferUsages | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    component.indices = device.createBuffer(
        sizeOfIndexBuffer, 
        bufferUsages | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    VkCommandBuffer commandBuffer = device.startSingleSubmit();

    VkBufferCopy vertexRegion = {};
    vertexRegion.size = sizeOfVertexBuffer;
    vkCmdCopyBuffer(commandBuffer, vertexStageBuffer.buffer, component.vertices.buffer, 1, &vertexRegion);

    VkBufferCopy indexRegion = {};
    indexRegion.size = sizeOfIndexBuffer;
    vkCmdCopyBuffer(commandBuffer, indexStageBuffer.buffer, component.indices.buffer, 1, &indexRegion);

    device.flushSingleSubmit(commandBuffer);

    device.destroyBuffer(indexStageBuffer);
    device.destroyBuffer(vertexStageBuffer);

    VkAccelerationStructureGeometryTrianglesDataKHR triangles = {};
    triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.maxVertex = uint32_t(mesh.positions.size());
    triangles.vertexStride = uint32_t(mesh.vertexBuffer.getStride());
    triangles.indexData.deviceAddress = device.getDeviceAddress(component.indices);
    triangles.vertexData.deviceAddress = device.getDeviceAddress(component.vertices);

    VkAccelerationStructureGeometryKHR geometry = {};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles = triangles;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    component.accelStruct.create(device, buildInfo, uint32_t(mesh.indices.size() / 3u));

    return component;
}

void Renderer::destroyBLAS(RTGeometry& geometry) {
    geometry.accelStruct.destroy(device);
    device.destroyBuffer(geometry.vertices);
    device.destroyBuffer(geometry.indices);
}

AccelerationStructure Renderer::createTLAS(VkAccelerationStructureInstanceKHR* instances, size_t count) {
    assert(instances);
    assert(count);

    const size_t bufferSize = sizeof(VkAccelerationStructureInstanceKHR) * count;

    auto buffer = device.createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    std::memcpy(device.getMappedPointer(buffer), instances, bufferSize);

    VkAccelerationStructureGeometryInstancesDataKHR instanceData = {};
    instanceData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instanceData.data.deviceAddress = device.getDeviceAddress(buffer);

    VkAccelerationStructureGeometryKHR geometry = {};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances = instanceData;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    AccelerationStructure tlas = {};
    tlas.create(device, buildInfo, uint32_t(count));

    device.destroyBuffer(buffer);

    return tlas;
}

} // raekor