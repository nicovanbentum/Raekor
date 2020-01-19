#pragma once

#include "VKContext.h"

namespace Raekor {
namespace VK {

class Swapchain {
public:
    Swapchain(const Context& context, glm::vec2 resolution, VkPresentModeKHR mode);
    ~Swapchain();
    void recreate(const Context& context, glm::vec2 resolution, VkPresentModeKHR mode);
    void setupFrameBuffers(const Context& context, VkRenderPass renderPass, const std::vector<VkImageView>& attachments);

private:
    bool created;
    VkDevice device;
    VkExtent2D extent;
    VkSwapchainKHR swapchain;
    VkFormat swapChainImageFormat;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    
};

} // VK
} // Raekor