#pragma once
#include "VKBase.h"
#include "VKStructs.h"
#include "VKTexture.h"
#include "VKFrameBuffer.h"
#include "VKPipeline.h"

namespace RK::VK {

 class Shader;

class Device {
    friend class SwapChain;
    friend class ImGuiPass;

    Device() = delete;
    Device(Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&) = delete;
    Device& operator=(Device&&) = delete;

public:
    explicit Device(SDL_Window* window);
    ~Device();

    /* Useful for passing our Device directly to Vulkan functions. */
    operator VkDevice() const { return m_Device; }
    
    VmaAllocator GetAllocator() { return m_Allocator; }
    const PhysicalDevice& GetPhysicalDevice() const { return m_PhysicalDevice; }

    VkQueue GetQueue() { return m_Queue; }
    uint32_t GetQueueFamilyIndex() { return m_QueueFamilyIndex; }

    [[nodiscard]] VkCommandBuffer StartSingleSubmit() const;
    void FlushSingleSubmit(VkCommandBuffer commandBuffer) const;

    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const;
    void CopyBufferToImage(const Buffer& buffer, Texture& texture, uint32_t width, uint32_t height, uint32_t layerCount = 1) const;
    
    void GenerateMipmaps(Texture& texture) const;
    void TransitionImageLayout(const Texture& texture, VkImageLayout oldLayout, VkImageLayout newLayout) const;

    void AllocateDescriptorSet(uint32_t count, VkDescriptorSetLayout* layouts, VkDescriptorSet* sets, const void* pNext = nullptr) const;
    void FreeDescriptorSet(uint32_t count, VkDescriptorSet* sets) const;

    [[nodiscard]] Shader CreateShader(const std::string& filepath, VkShaderStageFlagBits stageFlags);
    void DestroyShader(Shader& shader);
    
    [[nodiscard]] Texture CreateTexture(const Texture::Desc& desc);
    void DestroyTexture(Texture& texture);
    
    [[nodiscard]] Sampler CreateSampler(const Sampler::Desc& desc);
    void DestroySampler(Sampler& sampler);
    
    [[nodiscard]] Buffer CreateBuffer(size_t size, VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage);
    void DestroyBuffer(const Buffer& buffer);
    
    [[nodiscard]] AccelStruct CreateAccelStruct(VkAccelerationStructureBuildGeometryInfoKHR& buildInfo, const uint32_t primitiveCount);
    void DestroyAccelStruct(AccelStruct& accelStruct);
    
    [[nodiscard]] FrameBuffer CreateFrameBuffer(const FrameBuffer::Desc& desc);
    void DestroyFrameBuffer(const FrameBuffer& frameBuffer);
    
    [[nodiscard]] GraphicsPipeline CreateGraphicsPipeline(const GraphicsPipeline::Desc& desc, const FrameBuffer& framebuffer, VkPipelineLayout layout);
    void DestroyGraphicsPipeline(const GraphicsPipeline& pipeline);
    
    [[nodiscard]] VkImageView CreateView(Texture& texture, uint8_t swizzle = TEXTURE_SWIZZLE_RGBA, uint32_t mipLevel = 0);
    /* Use vkDestroyImageView or let DestroyTexture clean up the views.*/

    [[nodiscard]] VkBufferView CreateView(Buffer& buffer, VkFormat format, VkDeviceSize offset, VkDeviceSize range = VK_WHOLE_SIZE);
    /* Use vkDestroyBufferView or let DestroyBuffer clean up the views. */

    void SetDebugName(const Texture& texture, const std::string& name);
    
    void* MapPointer(const Texture& texture);
    void UnmapPointer(const Texture& texture);

    template<typename T>
    T GetMappedPointer(const Buffer& buffer) {
        VmaAllocationInfo info = {};
        vmaGetAllocationInfo(m_Allocator, buffer.allocation, &info);
        return static_cast<T>(info.pMappedData);
    }

    VkDeviceAddress GetDeviceAddress(VkBuffer buffer) const;
    VkDeviceAddress GetDeviceAddress(const Buffer& buffer) const;
    VkDeviceAddress GetDeviceAddress(VkAccelerationStructureKHR accelerationStructure) const;

    const PhysicalDevice::Properties& GetPhysicalProperties() const { return m_PhysicalDevice.m_Properties; }

private:
    Instance m_Instance;
    PhysicalDevice m_PhysicalDevice;
    VkDevice m_Device;

    VkQueue m_Queue;
    uint32_t m_QueueFamilyIndex = 0;

    VkCommandPool m_CommandPool;
    VkDescriptorPool m_DescriptorPool;

    VmaAllocator m_Allocator;
    VkPhysicalDeviceMemoryProperties m_MemoryProperties;
};

} // Raekor::VK