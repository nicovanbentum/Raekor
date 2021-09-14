#include "pch.h"
#include "VKRenderer.h"
#include "mesh.h"
#include "VKAccelerationStructure.h"
#include "VKExtensions.h"
#include "scene.h"
#include "util.h"

namespace Raekor {
namespace VK {

Renderer::~Renderer() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(context.device, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(context.device, renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(context.device, inFlightFences[i], nullptr);
    }

    for (const auto& texture : textures) {
        vmaDestroyImage(context.device.getAllocator(), texture.image, texture.allocation);
        vkDestroyImageView(context.device, texture.view, nullptr);
        vkDestroySampler(context.device, texture.sampler, nullptr);
    }

    bindlessTextures.destroy(context.device);

    TLAS.destroy(context.device);
    swapchain.destroy(context.device);
    pathTracePass.destroy(context.device);
    context.device.destroyBuffer(instanceBuffer, instanceAllocation);
    context.device.destroyBuffer(materialBuffer, materialAllocation);
}

void Renderer::initialize(Scene& scene) {
    pathTracePass.initialize(context, swapchain, TLAS, instanceBuffer, materialBuffer, bindlessTextures);
}

void Renderer::updateMaterials(Assets& assets, Scene& scene) {
    auto materialView = scene.view<Material>();
    std::vector<RTMaterial> materials(materialView.size());

    for (auto& [entity, material] : materialView.each()) {
        uint32_t index = entt::entt_traits<entt::entity>::to_entity(entity);
        
        auto& buffer = materials[index];
        buffer.albedo = material.baseColour;

        buffer.textures.x = addBindlessTexture(context.device, assets.get<TextureAsset>(material.albedoFile));
        buffer.textures.y = addBindlessTexture(context.device, assets.get<TextureAsset>(material.normalFile));
        buffer.textures.z = addBindlessTexture(context.device, assets.get<TextureAsset>(material.mrFile));

        buffer.properties.x = material.metallic;
        buffer.properties.y = material.roughness;
        buffer.properties.z = 1.0f;
    }

    const auto materialBufferSize = materials.size() * sizeof(materials[0]);

    std::tie(materialBuffer, materialAllocation) = context.device.createBuffer(
        materialBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    auto mappedPtr = context.device.getMappedPointer(materialAllocation);
    memcpy(mappedPtr, materials.data(), materialBufferSize);
}

void Renderer::updateAccelerationStructures(Scene& scene) {
    auto meshes = scene.view<Mesh, Transform, VK::RTGeometry>();

    if (meshes.size_hint() < 1) return;

    std::vector<VkAccelerationStructureInstanceKHR> deviceInstances;

    uint32_t customIndex = 0; 
    for (auto& [entity, mesh, transform, geometry] : meshes.each()) {
        auto deviceAddress = context.device.getDeviceAddress(geometry.accelerationStructure.accelerationStructure);

        VkAccelerationStructureInstanceKHR instance = {};
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.mask = 0xFF;
        instance.instanceCustomIndex = customIndex++;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.accelerationStructureReference = deviceAddress;
        instance.transform = VkTransformMatrix(transform.worldTransform);

        deviceInstances.push_back(instance);
    }

    TLAS.destroy(context.device);
    TLAS = createTLAS(deviceInstances.data(), deviceInstances.size());

    struct Instance {
        glm::ivec4 materialIndex;
        glm::mat4 localToWorldTransform;
        VkDeviceAddress indexBufferAddress;
        VkDeviceAddress vertexBufferAddress;
    };

    std::vector<Instance> hostInstances;

    for (auto& [entity, mesh, transform, geometry] : meshes.each()) {
        Instance instance = {};
        instance.materialIndex.x = entt::entt_traits<entt::entity>::to_entity(mesh.material);
        instance.localToWorldTransform = transform.worldTransform;
        instance.indexBufferAddress = context.device.getDeviceAddress(geometry.indexBuffer);
        instance.vertexBufferAddress = context.device.getDeviceAddress(geometry.vertexBuffer);

        hostInstances.push_back(instance);
    }

    std::tie(instanceBuffer, instanceAllocation) = context.device.createBuffer(
        sizeof(Instance) * meshes.size_hint(), 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    auto mappedPtr = context.device.getMappedPointer(instanceAllocation);

    memcpy(mappedPtr, hostInstances.data(), hostInstances.size() * sizeof(Instance));
}

void Renderer::render(const Viewport& viewport, Scene& scene) {
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context.device, swapchain, UINT64_MAX, imageAvailableSemaphores[current_frame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain(vsync);
        return;
    }

    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(context.device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(swapchain.blitBuffer, &beginInfo);

    // TODO: implement actual texture class that tracks the image layouts and abstract the image barriers

    if (pathTracePass.finalImageLayout != VK_IMAGE_LAYOUT_GENERAL) {
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; // TODO: optimize
        barrier.oldLayout = pathTracePass.finalImageLayout;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.image = pathTracePass.finalImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(swapchain.blitBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        pathTracePass.finalImageLayout = barrier.newLayout;
    }

    pathTracePass.recordCommands(context, viewport, swapchain.blitBuffer, bindlessTextures);

    {   // transition patrh trace image to transfer src layout
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; // TODO: optimize
        barrier.oldLayout = pathTracePass.finalImageLayout;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.image = pathTracePass.finalImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(swapchain.blitBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        pathTracePass.finalImageLayout = barrier.newLayout;
    }

    {   // transition swapchain to transfer destination layout
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.image = swapchain.images[imageIndex];;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(swapchain.blitBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    VkImageBlit region = {};

    region.srcOffsets[1] = { (int32_t)viewport.size.x, (int32_t)viewport.size.y, 1 };
    region.srcSubresource.layerCount = 1;
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    region.dstOffsets[1] = { (int32_t)viewport.size.x, (int32_t)viewport.size.y, 1 };
    region.dstSubresource.layerCount = 1;
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    vkCmdBlitImage(swapchain.blitBuffer, pathTracePass.finalImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchain.images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST);

    {   // transition swapchain image to present layout
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.image = swapchain.images[imageIndex];;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(swapchain.blitBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    vkEndCommandBuffer(swapchain.blitBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pCommandBuffers = &swapchain.blitBuffer;
    submitInfo.pSignalSemaphores = &renderFinishedSemaphores[current_frame];

    vkQueueSubmit(context.device.queue, 1, &submitInfo, NULL);

    imagesInFlight[imageIndex] = inFlightFences[current_frame];

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[current_frame] };
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[current_frame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSemaphore waits[] = { imageAvailableSemaphores[current_frame], renderFinishedSemaphores[current_frame] };

    VkSwapchainKHR swapChains[] = { swapchain };

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 2;
    presentInfo.pWaitSemaphores = waits;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(context.device.queue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain(vsync);
    }  else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    vkQueueWaitIdle(context.device.queue);

    vkWaitForFences(context.device, 1, &inFlightFences[current_frame], VK_TRUE, UINT64_MAX);
    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}


Renderer::Renderer(SDL_Window* window) : context(window) {
    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    VkPresentModeKHR mode = vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;

    if (!swapchain.create(context, { w, h }, mode)) {
        throw std::runtime_error("Failed to create swapchain.");
    }

    setupSyncObjects();

    bindlessTextures.create(context, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
}



void Renderer::setupSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(swapchain.images.size(), VK_NULL_HANDLE);

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]);
        vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]);
        vkCreateFence(context.device, &fenceInfo, nullptr, &inFlightFences[i]);
    }
}

void Renderer::recreateSwapchain(bool useVsync) {
    vsync = useVsync;
    vkQueueWaitIdle(context.device.queue);
    int w, h;
    SDL_GetWindowSize(context.window, &w, &h);
    uint32_t flags = SDL_GetWindowFlags(context.window);
    while (flags & SDL_WINDOW_MINIMIZED) {
        flags = SDL_GetWindowFlags(context.window);
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {}
    }
    // recreate the swapchain
    VkPresentModeKHR mode = useVsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
    swapchain.destroy(context.device);
    if (!swapchain.create(context, { w, h }, mode)) {
        std::puts("WTF SWAP CHAIN FAILED??");
    }

    pathTracePass.destroyFinalTexture(context.device);
    pathTracePass.createFinalTexture(context.device, { w, h });
    pathTracePass.updateDescriptorSet(context.device, TLAS, instanceBuffer, materialBuffer);
}

int32_t Renderer::addBindlessTexture(Device& device, const std::shared_ptr<TextureAsset>& asset) {
    if (!asset) {
        return -1;
    }

    const glm::uvec2 dimensions = { asset->header()->dwWidth, asset->header()->dwHeight };
    uint32_t mipmapCount = asset->header()->dwMipMapCount - 2; // Reload texture assets to account for 4x4 DXT

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_BC3_UNORM_BLOCK; // DXT5
    imageInfo.extent = { dimensions.x, dimensions.y, 1 };
    imageInfo.mipLevels = mipmapCount;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT 
        | VK_IMAGE_USAGE_TRANSFER_DST_BIT 
        | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    Texture2 texture = {};
    ThrowIfFailed(vmaCreateImage(device.getAllocator(), &imageInfo, &allocInfo, &texture.image, &texture.allocation, nullptr));

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_BC3_UNORM_BLOCK;
    viewInfo.image = texture.image;
    viewInfo.subresourceRange.levelCount = mipmapCount;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    vkCreateImageView(device, &viewInfo, nullptr, &texture.view);

    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(mipmapCount);

    vkCreateSampler(device, &samplerInfo, nullptr, &texture.sampler);

    auto [buffer, allocation] = device.createBuffer(
        asset->getDataSize(), 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VMA_MEMORY_USAGE_CPU_ONLY
    );

    auto mappedPtr = device.getMappedPointer(allocation);

    memcpy(mappedPtr, asset->getData(), asset->getDataSize());

    device.transitionImageLayout(
        texture.image, 
        VK_FORMAT_BC3_UNORM_BLOCK, 
        mipmapCount, 1, 
        VK_IMAGE_LAYOUT_UNDEFINED, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    std::vector<VkBufferImageCopy> regions(mipmapCount);

    uint32_t mip = 0, bufferOffset = 0;

    for (auto& region : regions) {
        glm::uvec2 mipDimensions = { 
            std::max(dimensions.x >> mip, 1u), 
            std::max(dimensions.y >> mip, 1u) 
        };

        region.bufferOffset = bufferOffset;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = mip++;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { mipDimensions.x, mipDimensions.y, 1 };
        
        bufferOffset +=  mipDimensions.x * mipDimensions.y;
    }

    auto commandBuffer = device.startSingleSubmit();

    vkCmdCopyBufferToImage(
        commandBuffer, 
        buffer, texture.image, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
        regions.size(), regions.data()
    );

    device.flushSingleSubmit(commandBuffer);

    device.transitionImageLayout(
        texture.image, 
        VK_FORMAT_BC3_UNORM_BLOCK, 
        mipmapCount, 1, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    // TODO: load mips in one go, refactor Device to put everything in a single command buffer

    device.destroyBuffer(buffer, allocation);
    textures.push_back(texture);

    VkDescriptorImageInfo descriptorInfo = {};
    descriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptorInfo.imageView = texture.view;
    descriptorInfo.sampler = texture.sampler;

    return bindlessTextures.write(device, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorInfo);
}

void Renderer::reloadShaders() {
    pathTracePass.reloadShadersFromDisk(context.device);
}

RTGeometry Renderer::createBLAS(Mesh& mesh) {
    auto vertices = mesh.getInterleavedVertices();
    const auto sizeOfVertexBuffer = vertices.size() * sizeof(vertices[0]);
    const auto sizeOfIndexBuffer = mesh.indices.size() * sizeof(mesh.indices[0]);

    auto [vertexStageBuffer, vertexStageAllocation] = context.device.createBuffer(sizeOfVertexBuffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    auto [indexStageBuffer, indexStageAllocation] = context.device.createBuffer(sizeOfIndexBuffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    memcpy(context.device.getMappedPointer(vertexStageAllocation), vertices.data(), sizeOfVertexBuffer);
    memcpy(context.device.getMappedPointer(indexStageAllocation), mesh.indices.data(), sizeOfIndexBuffer);

    const auto bufferUsages = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    auto [vertexBuffer, vertexAllocation] = context.device.createBuffer(sizeOfVertexBuffer, bufferUsages | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    auto [indexBuffer, indexAllocation] = context.device.createBuffer(sizeOfIndexBuffer, bufferUsages | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    VkCommandBuffer commandBuffer = context.device.startSingleSubmit();

    VkBufferCopy vertexRegion = {};
    vertexRegion.size = sizeOfVertexBuffer;
    vkCmdCopyBuffer(commandBuffer, vertexStageBuffer, vertexBuffer, 1, &vertexRegion);

    VkBufferCopy indexRegion = {};
    indexRegion.size = sizeOfIndexBuffer;
    vkCmdCopyBuffer(commandBuffer, indexStageBuffer, indexBuffer, 1, &indexRegion);

    context.device.flushSingleSubmit(commandBuffer);

    vmaDestroyBuffer(context.device.getAllocator(), indexStageBuffer, indexStageAllocation);
    vmaDestroyBuffer(context.device.getAllocator(), vertexStageBuffer, vertexStageAllocation);

    VkAccelerationStructureGeometryTrianglesDataKHR triangles = {};
    triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.maxVertex = static_cast<uint32_t>(mesh.positions.size());
    triangles.vertexStride = static_cast<uint32_t>(mesh.vertexBuffer.getStride());
    triangles.indexData.deviceAddress = context.device.getDeviceAddress(indexBuffer);
    triangles.vertexData.deviceAddress = context.device.getDeviceAddress(vertexBuffer);

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

    AccelerationStructure bottomLevelAS;
    bottomLevelAS.create(context.device, buildInfo, static_cast<uint32_t>(mesh.indices.size() / 3u));

    RTGeometry component = {};
    component.indexBuffer = indexBuffer;
    component.vertexBuffer = vertexBuffer;
    component.indexAllocation = indexAllocation;
    component.vertexAllocation = vertexAllocation;
    component.accelerationStructure = bottomLevelAS;

    return component;
}

void Renderer::destroyBLAS(RTGeometry& geometry) {
    geometry.accelerationStructure.destroy(context.device);
    vmaDestroyBuffer(context.device.getAllocator(), geometry.vertexBuffer, geometry.vertexAllocation);
    vmaDestroyBuffer(context.device.getAllocator(), geometry.indexBuffer, geometry.indexAllocation);
}

AccelerationStructure Renderer::createTLAS(VkAccelerationStructureInstanceKHR* instances, size_t count) {
    assert(instances);
    assert(count);

    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(VkAccelerationStructureInstanceKHR) * count;
    bufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    if (vmaCreateBuffer(context.device.getAllocator(), &bufferInfo, &allocInfo, &buffer, &allocation, &allocationInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    std::memcpy(allocationInfo.pMappedData, instances, sizeof(VkAccelerationStructureInstanceKHR) * count);

    VkAccelerationStructureGeometryInstancesDataKHR instanceData = {};
    instanceData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instanceData.data.deviceAddress = context.device.getDeviceAddress(buffer);

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
    tlas.create(context.device, buildInfo, static_cast<uint32_t>(count));

    vmaDestroyBuffer(context.device.getAllocator(), buffer, allocation);

    return tlas;
}

} // vk
} // raekor