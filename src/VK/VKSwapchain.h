#pragma once

namespace Raekor::VK {

class Device;

class Swapchain {
public:
    operator VkSwapchainKHR() const { return swapchain; }

    void create(const Device& device, glm::ivec2 resolution, VkPresentModeKHR mode);
    void destroy(VkDevice device);

    const VkExtent2D& getExtent() const;
    const uint32_t getImageCount() const;

private:
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;

public:
    VkExtent2D extent;
    VkFormat imageFormat;
    std::vector<VkImage> images;

    std::vector<VkCommandBuffer> submitBuffers;
    
};

} // Raekor