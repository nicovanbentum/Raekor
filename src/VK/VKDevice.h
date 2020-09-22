#pragma once
#include "VKBase.h"

namespace Raekor {
namespace VK {

class Device {
    struct Queues {
        std::optional<uint32_t> graphics;
        std::optional<uint32_t> present;
        bool isComplete() { return graphics.has_value() && present.has_value(); }
    };

public:
    Device(const Instance& instance, const PhysicalDevice& physicalDevice);
    ~Device();
    operator VkDevice() { return device; }
    operator VkDevice() const { return device; }
    const Queues& getQueues() const { return qindices; }

    VkCommandBuffer beginSingleTimeCommands() const;
    void endSingleTimeCommands(VkCommandBuffer commandBuffer) const;

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const;
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount = 1) const;
    
    void transitionImageLayout(VkImage image, VkFormat format, uint32_t mipLevels, uint32_t layerCount, VkImageLayout oldLayout, VkImageLayout newLayout) const;
    VkImageView createImageView(VkImage image, VkFormat format, VkImageViewType type, VkImageAspectFlags aspectFlags, uint32_t mipLevels, uint32_t layerCount) const;
    void generateMipmaps(VkImage image, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) const;

    void allocateDescriptorSet(uint32_t count, VkDescriptorSetLayout* layouts, VkDescriptorSet* sets) const;
    void freeDescriptorSet(uint32_t count, VkDescriptorSet* sets) const;

    std::tuple<VkBuffer, VmaAllocation, VmaAllocationInfo> createStagingBuffer(VmaAllocator allocator, size_t sizeInBytes) const;

private:
    Queues qindices;
    VkDevice device;
public:
    VkQueue presentQueue;
    VkQueue graphicsQueue;
    VkCommandPool commandPool;
    VkDescriptorPool descriptorPool;
private:
    VkPhysicalDeviceMemoryProperties memProperties;
};

}
}