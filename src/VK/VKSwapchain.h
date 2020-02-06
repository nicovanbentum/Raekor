#pragma once

#include "VKContext.h"

namespace Raekor {
namespace VK {

class Swapchain {
public:
    Swapchain();
    Swapchain(const Context& context, glm::vec2 resolution, VkPresentModeKHR mode);
    ~Swapchain();

    void destruct();
    inline const VkExtent2D& getExtent() const { return extent; }
    void recreate(const Context& context, glm::vec2 resolution, VkPresentModeKHR mode);
    void setupFrameBuffers(const Context& context, VkRenderPass renderPass, const std::vector<VkImageView>& attachments);

    operator VkSwapchainKHR() const { return swapchain; }

private:
    bool created;
    VkDevice device;
    VkSwapchainKHR swapchain;

public:
    VkExtent2D extent;
    VkFormat imageFormat;
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    std::vector<VkFramebuffer> framebuffers;
    
};

} // VK
} // Raekor