#include "pch.h"
#include "VKRenderer.h"
#include "mesh.h"
#include "VKAccelerationStructure.h"
#include "VKExtensions.h"
#include "scene.h"
#include "util.h"

namespace Raekor {
namespace VK {

void Renderer::reloadShaders() {
    
}

Renderer::~Renderer() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(context.device, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(context.device, renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(context.device, inFlightFences[i], nullptr);
    }

    swapchain.destroy(context.device);
}

void Renderer::render(Scene& scene) {
    auto meshes = scene.view<Mesh, Transform, VK::RTGeometry>();
    std::vector<VkAccelerationStructureInstanceKHR> instances;

    for (auto& [entity, mesh, transform, geometry] : meshes.each()) {
    auto deviceAddress = context.device.getDeviceAddress(geometry.accelerationStructure.accelerationStructure);

    VkAccelerationStructureInstanceKHR instance = {};
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instance.mask = 0xFF;
    instance.instanceCustomIndex = 0;
    instance.instanceShaderBindingTableRecordOffset = 0;
    instance.accelerationStructureReference = deviceAddress;
    instance.transform = VkTransformMatrix(transform.worldTransform);

    instances.push_back(instance);
    }

    if (!instances.empty()) {
        TLAS.destroy(context.device);
        TLAS = createTLAS(instances.data(), instances.size());
    }


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

    pathTracePass.initialize(context);
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
}

RTGeometry Renderer::createBLAS(Mesh& mesh) {
    auto vertices = mesh.getInterleavedVertices();
    const auto sizeOfVertexBuffer = vertices.size() * sizeof(vertices[0]);
    const auto sizeOfIndexBuffer = mesh.indices.size() * sizeof(mesh.indices[0]);

    auto [vertexStageBuffer, vertexStageAllocation] = context.device.createBuffer(sizeOfVertexBuffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, true);
    auto [indexStageBuffer, indexStageAllocation] = context.device.createBuffer(sizeOfIndexBuffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, true);

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