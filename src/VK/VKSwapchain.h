#pragma once

#include "VKContext.h"

namespace Raekor {
namespace VK {

class Swapchain {
public:
    Swapchain() = default;
    Swapchain(const Context& context, glm::vec2 resolution, VkPresentModeKHR mode);

    [[nodiscard]] bool create(const Context& context, glm::vec2 resolution, VkPresentModeKHR mode);
    void destroy(VkDevice device);

    operator VkSwapchainKHR() const { return swapchain; }
    inline const VkExtent2D& getExtent() const { return extent; }
    void setupFrameBuffers(const Context& context, VkRenderPass renderPass, const std::vector<VkImageView>& attachments);

private:
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;

public:
    VkExtent2D extent;
    VkFormat imageFormat;
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    std::vector<VkFramebuffer> framebuffers;

    VkCommandBuffer blitBuffer;
    
};

} // VK
} // Raekor