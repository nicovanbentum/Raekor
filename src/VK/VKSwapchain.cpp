#include "pch.h"
#include "VKSwapchain.h"

namespace Raekor {
namespace VK {

Swapchain::Swapchain(const Context& context, glm::vec2 resolution, VkPresentModeKHR mode) {
    if (!create(context, resolution, mode)) {
        std::puts("failed to create swap chain");
    }
}

///////////////////////////////////////////////////////////////////////

bool Swapchain::create(const Context& context, glm::vec2 resolution, VkPresentModeKHR mode)
{
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    VkSurfaceKHR surface = context.instance.getSurface();
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.PDevice, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(context.PDevice, surface, &formatCount, nullptr);

    if (formatCount) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(context.PDevice, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(context.PDevice, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(context.PDevice, surface, &presentModeCount, details.presentModes.data());
    }

    const std::vector<VkSurfaceFormatKHR> availableFormats = details.formats;
    const std::vector<VkPresentModeKHR> availablePresentModes = details.presentModes;
    const VkSurfaceCapabilitiesKHR capabilities = details.capabilities;
    //TODO: fill the vector
    VkSurfaceFormatKHR surface_format = availableFormats[0];
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = availableFormat;
        }
    }

    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == mode) {
            present_mode = availablePresentMode;
        }
    }

    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    }
    else {
        VkExtent2D actualExtent = {
            static_cast<uint32_t>(resolution.x),
            static_cast<uint32_t>(resolution.y)
        };
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        extent = actualExtent;
    }

    uint32_t imageCount = details.capabilities.minImageCount + 1;
    if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount) {
        imageCount = details.capabilities.maxImageCount;
    }


    VkSwapchainCreateInfoKHR sc_info = {};
    sc_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sc_info.surface = surface;

    sc_info.minImageCount = imageCount;
    sc_info.imageFormat = surface_format.format;
    sc_info.imageColorSpace = surface_format.colorSpace;
    sc_info.imageExtent = extent;
    sc_info.imageArrayLayers = 1;
    sc_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = {
        context.device.getQueues().graphics.value(),
        context.device.getQueues().present.value()
    };

    if (context.device.getQueues().graphics.value() != context.device.getQueues().present.value()) {
        sc_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        sc_info.queueFamilyIndexCount = 2;
        sc_info.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        sc_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        sc_info.queueFamilyIndexCount = 0; // Optional
        sc_info.pQueueFamilyIndices = nullptr; // Optional
    }

    sc_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sc_info.preTransform = details.capabilities.currentTransform;
    sc_info.presentMode = present_mode;
    sc_info.clipped = VK_TRUE;
    sc_info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(context.device, &sc_info, nullptr, &swapchain) != VK_SUCCESS) {
        return false;
    }

    vkGetSwapchainImagesKHR(context.device, swapchain, &imageCount, nullptr);
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(context.device, swapchain, &imageCount, images.data());
    imageFormat = surface_format.format;

    views.resize(images.size());
    for (size_t i = 0; i < images.size(); i++) {
        VkImageViewCreateInfo iv_info = {};
        iv_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        iv_info.image = images[i];
        iv_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        iv_info.format = imageFormat;
        iv_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        iv_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        iv_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        iv_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        iv_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        iv_info.subresourceRange.baseMipLevel = 0;
        iv_info.subresourceRange.levelCount = 1;
        iv_info.subresourceRange.baseArrayLayer = 0;
        iv_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(context.device, &iv_info, nullptr, &views[i]) != VK_SUCCESS) {
            return false;
        }
    }

    VkCommandBufferAllocateInfo blitBufferInfo = {};
    blitBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    blitBufferInfo.commandPool = context.device.commandPool;
    blitBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    blitBufferInfo.commandBufferCount = 1;

    vkAllocateCommandBuffers(context.device, &blitBufferInfo, &blitBuffer);

    return true;
}

///////////////////////////////////////////////////////////////////////

void Swapchain::destroy(VkDevice device) {
    for (auto& framebuffer : framebuffers)
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    for (auto& view : views) {
        vkDestroyImageView(device, view, nullptr);
    }
    vkDestroySwapchainKHR(device, swapchain, nullptr);
}

///////////////////////////////////////////////////////////////////////

void Swapchain::setupFrameBuffers(const Context& context, VkRenderPass renderPass, const std::vector<VkImageView>& pAttachments) {
    framebuffers.resize(views.size());
    for (size_t i = 0; i < views.size(); i++) {
        std::vector<VkImageView> attachments = { views[i] };
        for (auto& attachment : pAttachments) {
            attachments.push_back(attachment);
        }

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(context.device, &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vk framebuffer");
        }
    }
}

} // VK
} // Raekor