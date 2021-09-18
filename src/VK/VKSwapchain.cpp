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

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device.physicalDevice, surface, &presentModeCount, details.presentModes.data());
    }

    const std::vector<VkSurfaceFormatKHR> availableFormats = details.formats;
    const std::vector<VkPresentModeKHR> availablePresentModes = details.presentModes;
    const VkSurfaceCapabilitiesKHR capabilities = details.capabilities;
    
    //TODO: fill the vector
    VkSurfaceFormatKHR surfaceFormat = availableFormats[0];
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = availableFormat;
        }
    }

    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == mode) {
            presentMode = availablePresentMode;
        }
    }

    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    }
    else {
        VkExtent2D actualExtent = {
            uint32_t(resolution.x),
            uint32_t(resolution.y)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        extent = actualExtent;
    }

    uint32_t imageCount = details.capabilities.minImageCount + 1;
    if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount) {
        imageCount = details.capabilities.maxImageCount;
    }


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
    views.resize(images.size());
    
    imageFormat = surfaceFormat.format;

    for (size_t i = 0; i < images.size(); i++) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.image = images[i];
        viewInfo.format = imageFormat;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        ThrowIfFailed(vkCreateImageView(device, &viewInfo, nullptr, &views[i]));
    }

    VkCommandBufferAllocateInfo blitBufferInfo = {};
    blitBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    blitBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    blitBufferInfo.commandPool = device.commandPool;
    blitBufferInfo.commandBufferCount = 1;

    vkAllocateCommandBuffers(device, &blitBufferInfo, &blitBuffer);
}



void Swapchain::destroy(VkDevice device) {
    for (auto& framebuffer : framebuffers)
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    for (auto& view : views) {
        vkDestroyImageView(device, view, nullptr);
    }
    vkDestroySwapchainKHR(device, swapchain, nullptr);
}



const VkExtent2D& Swapchain::getExtent() const { 
    return extent; 
}



void Swapchain::createFramebuffers(const Device& device, VkRenderPass renderPass, const std::vector<VkImageView>& pAttachments) {
    framebuffers.resize(views.size());
    for (size_t i = 0; i < views.size(); i++) {
        std::vector<VkImageView> attachments = { views[i] };
        for (auto& attachment : pAttachments) {
            attachments.push_back(attachment);
        }

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = uint32_t(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;

        ThrowIfFailed(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]));
    }
}

} // Raekor