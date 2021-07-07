#include "pch.h"
#include "VKRenderer.h"
#include "mesh.h"

namespace Raekor {
namespace VK {

    void Renderer::reloadShaders() {
        if (vkDeviceWaitIdle(context.device) != VK_SUCCESS) {
            throw std::runtime_error("failed to wait for the gpu to idle");
        }
        // recompile and reload the shaders
        bool sucess = false;;
    }

    Renderer::~Renderer() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        // destroy/free pipeline related stuff
        vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);

        // destroy/free command buffer stuff
        std::array<VkCommandBuffer, 2> buffers = { maincmdbuffer};
        vkFreeCommandBuffers(context.device, context.device.commandPool, (uint32_t)buffers.size(), buffers.data());

        // destroy/free sync objects
        for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(context.device, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(context.device, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(context.device, inFlightFences[i], nullptr);
        }

        swapchain.destroy(context.device);
    }

    void Renderer::run() {
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(context.device, swapchain, UINT64_MAX, imageAvailableSemaphores[current_frame], VK_NULL_HANDLE, &imageIndex);
        
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain(vsync);
            return;
        }

        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(context.device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }

        imagesInFlight[imageIndex] = inFlightFences[current_frame];

        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[current_frame] };
        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[current_frame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        
        VkSwapchainKHR swapChains[] = { swapchain };
        
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &imageAvailableSemaphores[current_frame];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(context.device.presentQueue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            recreateSwapchain(vsync);
        }  else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        vkWaitForFences(context.device, 1, &inFlightFences[current_frame], VK_TRUE, UINT64_MAX);
        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    
    Renderer::Renderer(SDL_Window* window) : context(window) {
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        
        VkPresentModeKHR mode = vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
        
        if (!swapchain.create(context, { w,h }, mode)) {
            throw std::runtime_error("Failed to create swapchain.");
        }

        setupSyncObjects();
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
        vkQueueWaitIdle(context.device.graphicsQueue);
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

    void Renderer::createAccelerationStructure() {


    }

} // vk
} // raekor