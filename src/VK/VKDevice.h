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

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) const;
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const;

    void createImage(uint32_t w, uint32_t h, uint32_t mipLevels, uint32_t layers, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
        VkImage& image, VkDeviceMemory& imageMemory) const;
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount = 1) const;
    void transitionImageLayout(VkImage image, VkFormat format, uint32_t mipLevels, uint32_t layerCount, VkImageLayout oldLayout, VkImageLayout newLayout) const;
    VkImageView createImageView(VkImage image, VkFormat format, VkImageViewType type, VkImageAspectFlags aspectFlags, uint32_t mipLevels, uint32_t layerCount) const;
    void generateMipmaps(VkImage image, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) const;

    template<typename T>
    void uploadBuffer(const std::vector<T>& v, VkBufferUsageFlagBits usage, VkBuffer& outBuffer, VkDeviceMemory& outMemory) const {
        VkDeviceSize buffer_size = sizeof(T) * v.size();

        VkBuffer staging_buffer;
        VkDeviceMemory stage_mem;
        createBuffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer,
            stage_mem
        );

        void* data;
        vkMapMemory(device, stage_mem, 0, buffer_size, 0, &data);
        memcpy(data, v.data(), (size_t)buffer_size);
        vkUnmapMemory(device, stage_mem);

        createBuffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            outBuffer,
            outMemory
        );

        copyBuffer(staging_buffer, outBuffer, buffer_size);
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, stage_mem, nullptr);
    }

    void allocateDescriptorSet(uint32_t count, VkDescriptorSetLayout* layouts, VkDescriptorSet* sets) const;
    void freeDescriptorSet(uint32_t count, VkDescriptorSet* sets) const;

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