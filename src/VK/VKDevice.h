#pragma once
#include "VKBase.h"
#include "VKStructs.h"
#include "VKTexture.h"
#include "VKFrameBuffer.h"
#include "VKPipeline.h"

namespace Raekor::VK {

 class Shader;

class Device {
    friend class Swapchain;
    friend class ImGuiPass;

    Device() = delete;
    Device(Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&) = delete;
    Device& operator=(Device&&) = delete;

public:
    explicit Device(SDL_Window* window);
    ~Device();

    operator VkDevice() const { return device; }

    const PhysicalDevice& getPhysicalDevice() const { return physicalDevice; }

    VkQueue getQueue() { return queue; }
    uint32_t getQueueFamilyIndex() { return queueFamilyIndex; }

    [[nodiscard]] VkCommandBuffer startSingleSubmit() const;
    void flushSingleSubmit(VkCommandBuffer commandBuffer) const;

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const;
    void copyBufferToImage(const Buffer& buffer, Texture& texture, uint32_t width, uint32_t height, uint32_t layerCount = 1) const;
    
    void transitionImageLayout(const Texture& texture, VkImageLayout oldLayout, VkImageLayout newLayout) const;
    void generateMipmaps(VkImage image, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) const;

    void allocateDescriptorSet(uint32_t count, VkDescriptorSetLayout* layouts, VkDescriptorSet* sets, const void* pNext = nullptr) const;
    void freeDescriptorSet(uint32_t count, VkDescriptorSet* sets) const;

    VmaAllocator getAllocator() { return this->allocator; }

    [[nodiscard]] Shader createShader(const std::string& filepath);
    [[nodiscard]] Texture createTexture(const Texture::Desc& desc);
    [[nodiscard]] Sampler createSampler(const Sampler::Desc& desc);
    [[nodiscard]] VkImageView createView(Texture& texture, uint32_t mipLevel = 0);
    [[nodiscard]] Buffer createBuffer(size_t size, VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage);
    [[nodiscard]] GraphicsPipeline createGraphicsPipeline(const GraphicsPipeline::Desc& desc, const FrameBuffer& framebuffer);

    void destroyShader(Shader& shader);
    void destroySampler(Sampler& sampler);
    void destroyTexture(Texture& texture);
    void destroyBuffer(const Buffer& buffer);

    void setDebugName(const Texture& texture, const std::string& name);
    
    void* getMappedPointer(const Buffer& buffer);

    VkDeviceAddress getDeviceAddress(VkBuffer buffer) const;
    VkDeviceAddress getDeviceAddress(const Buffer& buffer) const;
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