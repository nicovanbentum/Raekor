#pragma once

namespace Raekor::VK {

class Device;

class Swapchain {
public:
    Swapchain() = default;
    operator VkSwapchainKHR() const { return swapchain; }

    void create(const Device& device, glm::ivec2 resolution, VkPresentModeKHR mode);
    void destroy(VkDevice device);

    const VkExtent2D& getExtent() const;
    void createFramebuffers(const Device& device, VkRenderPass renderPass, const std::vector<VkImageView>& attachments);

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

} // Raekor