#include "pch.h"
#include "VKSwapchain.h"

#include "VKDevice.h"
#include "VKUtil.h"

namespace Raekor::VK {

void Swapchain::create(const Device& device, glm::ivec2 resolution, VkPresentModeKHR mode) {
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    VkSurfaceKHR surface = device.instance.getSurface();

    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physicalDevice, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice, surface, &formatCount, nullptr);

    if (formatCount) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.physicalDevice, surface, &presentModeCount, nullptr);

    if (presentModeCount) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device.physicalDevice, surface, &presentModeCount, details.presentModes.data());
    }

    VkSurfaceFormatKHR surfaceFormat = details.formats[0];
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

    for (const auto& availableFormat : details.formats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && 
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = availableFormat;
        }
    }

    for (const auto& availablePresentMode : details.presentModes) {
        if (availablePresentMode == mode) {
            presentMode = availablePresentMode;
        }
    }

    if (details.capabilities.currentExtent.width != UINT32_MAX) {
        extent = details.capabilities.currentExtent;
    } else {
        VkExtent2D actualExtent = {
            uint32_t(resolution.x),
            uint32_t(resolution.y)
        };

        actualExtent.width = std::clamp(
            actualExtent.width, 
            details.capabilities.minImageExtent.width, 
            details.capabilities.maxImageExtent.width
        );

        actualExtent.height = std::clamp(
            actualExtent.height, 
            details.capabilities.minImageExtent.height, 
            details.capabilities.maxImageExtent.height
        );

        extent = actualExtent;
    }

    uint32_t imageCount = std::min(details.capabilities.minImageCount + 1, details.capabilities.maxImageCount);;

    VkSwapchainCreateInfoKHR swapchainInfo = {};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = surface;
    swapchainInfo.imageExtent = extent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = surfaceFormat.format;
    swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.preTransform = details.capabilities.currentTransform;
    swapchainInfo.presentMode = presentMode;
    swapchainInfo.clipped = VK_TRUE;

    ThrowIfFailed(vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain));

    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());
    
    imageFormat = surfaceFormat.format;

    VkCommandBufferAllocateInfo cmdBufferInfo = {};
    cmdBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufferInfo.commandPool = device.commandPool;
    cmdBufferInfo.commandBufferCount = imageCount;

    submitBuffers.resize(imageCount);

    vkAllocateCommandBuffers(device, &cmdBufferInfo, submitBuffers.data());
}



void Swapchain::destroy(VkDevice device) {
    vkDestroySwapchainKHR(device, swapchain, nullptr);
}



const VkExtent2D& Swapchain::getExtent() const { 
    return extent; 
}



const uint32_t Swapchain::getImageCount() const { 
    return uint32_t(images.size()); 
}

} // Raekor