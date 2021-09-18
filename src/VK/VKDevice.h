#pragma once
#include "VKBase.h"

namespace Raekor::VK {

class Device {
    friend class Swapchain;

    Device() = delete;
    Device(Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&) = delete;
    Device& operator=(Device&&) = delete;

public:
    explicit Device(SDL_Window* window);
    ~Device();

    operator VkDevice() { return device; }
    operator VkDevice() const { return device; }

    const PhysicalDevice& getPhysicalDevice() const { return physicalDevice; }

    VkQueue getQueue() { return queue; }
    uint32_t getQueueFamilyIndex() { return queueFamilyIndex; }

    VkCommandBuffer startSingleSubmit() const;
    void flushSingleSubmit(VkCommandBuffer commandBuffer) const;

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const;
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount = 1) const;
    
    void transitionImageLayout(VkImage image, VkFormat format, uint32_t mipLevels, uint32_t layerCount, VkImageLayout oldLayout, VkImageLayout newLayout) const;
    VkImageView createImageView(VkImage image, VkFormat format, VkImageViewType type, VkImageAspectFlags aspectFlags, uint32_t mipLevels, uint32_t layerCount) const;
    void generateMipmaps(VkImage image, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) const;

    void allocateDescriptorSet(uint32_t count, VkDescriptorSetLayout* layouts, VkDescriptorSet* sets, const void* pNext = nullptr) const;
    void freeDescriptorSet(uint32_t count, VkDescriptorSet* sets) const;

    VmaAllocator getAllocator() { return this->allocator; }

    [[nodiscard]] std::tuple<VkBuffer, VmaAllocation> createBuffer(size_t size, VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage);
    void destroyBuffer(VkBuffer buffer, VmaAllocation allocation);
    void* getMappedPointer(VmaAllocation allocation);

    VkDeviceAddress getDeviceAddress(VkBuffer buffer) const;
    VkDeviceAddress getDeviceAddress(VkAccelerationStructureKHR accelerationStructure) const;

    const PhysicalDevice::Properties& getPhysicalProperties() const { return physicalDevice.properties; }

private:
    Instance instance;
    PhysicalDevice physicalDevice;
    VkDevice device;

public:
    VkQueue queue;
    uint32_t queueFamilyIndex = 0;
    
    VkCommandPool commandPool;
    VkDescriptorPool descriptorPool;

private:
    VmaAllocator allocator;
    VkPhysicalDeviceMemoryProperties memProperties;
};

} // Raekor::VK