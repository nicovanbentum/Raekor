#include "pch.h"
#include "VKSwapchain.h"

#include "VKDevice.h"
#include "VKUtil.h"

namespace Raekor::VK {

void SwapChain::Create(const Device& device, glm::uvec2 resolution, VkPresentModeKHR mode) {
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    VkSurfaceKHR surface = device.m_Instance.GetSurface();

    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.m_PhysicalDevice, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.m_PhysicalDevice, surface, &formatCount, nullptr);

    if (formatCount) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device.m_PhysicalDevice, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.m_PhysicalDevice, surface, &presentModeCount, nullptr);

    if (presentModeCount) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device.m_PhysicalDevice, surface, &presentModeCount, details.presentModes.data());
    }

    VkSurfaceFormatKHR surfaceFormat = details.formats[0];
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

    for (const auto& availableFormat : details.formats)
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            surfaceFormat = availableFormat;

    for (const auto& availablePresentMode : details.presentModes)
        if (availablePresentMode == mode)
            presentMode = availablePresentMode;

    if (details.capabilities.currentExtent.width != 0xFFFFFFFF)
        extent.width = details.capabilities.currentExtent.width;
    else
        extent.width = std::min(resolution.x, details.capabilities.maxImageExtent.width);

    if (details.capabilities.currentExtent.height != 0xFFFFFFFF)
        extent.height = details.capabilities.currentExtent.height;
    else
        extent.height = std::min(resolution.x, details.capabilities.maxImageExtent.height);
    

    uint32_t imageCount = std::min(details.capabilities.minImageCount + 1, details.capabilities.maxImageCount);;

    VkSwapchainCreateInfoKHR swapchainInfo = {};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = surface;
    swapchainInfo.imageExtent = extent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = surfaceFormat.format;
    swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.preTransform = details.capabilities.currentTransform;
    swapchainInfo.presentMode = presentMode;
    swapchainInfo.clipped = VK_TRUE;

    gThrowIfFailed(vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &m_SwapChain));

    vkGetSwapchainImagesKHR(device, m_SwapChain, &imageCount, nullptr);
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(device, m_SwapChain, &imageCount, images.data());
    
    imageFormat = surfaceFormat.format;

    VkCommandBufferAllocateInfo cmdBufferInfo = {};
    cmdBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufferInfo.commandPool = device.m_CommandPool;
    cmdBufferInfo.commandBufferCount = imageCount;

    submitBuffers.resize(imageCount);

    vkAllocateCommandBuffers(device, &cmdBufferInfo, submitBuffers.data());
}


void SwapChain::Destroy(VkDevice device) {
    vkDestroySwapchainKHR(device, m_SwapChain, nullptr);
}


const VkExtent2D& SwapChain::GetExtent() const { 
    return extent; 
}


const uint32_t SwapChain::GetImageCount() const { 
    return uint32_t(images.size()); 
}

} // Raekor