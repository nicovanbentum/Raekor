#pragma once

#include "VKTexture.h"
#include "VKShader.h"

namespace Raekor {
namespace VK {

class BindlessDescriptorSet {
public:
    void create(const Device& device, VkDescriptorType type, VkShaderStageFlags stages);
    void destroy(const Device& device);

    uint32_t write(const Device& device, VkDescriptorType type, const VkDescriptorImageInfo& imageInfo);

    const VkDescriptorSet& getDescriptorSet() const { return set; }
    const VkDescriptorSetLayout& getLayout() const { return layout; }

private:
    uint32_t size = 0;
    std::vector<uint32_t> freeIndices;
    
private:
    VkDescriptorSet set;
    VkDescriptorPool pool;
    VkDescriptorSetLayout layout;

};

class UniformBuffer {
public:
    UniformBuffer() = default;
    UniformBuffer(Device& device, size_t size);

    void update(VmaAllocator allocator, const void* data, size_t size = VK_WHOLE_SIZE) const;
    inline const VkDescriptorBufferInfo* getDescriptor() const { return &descriptor; }
    void destroy(VmaAllocator allocator);

public:
    VkBuffer buffer;
    VmaAllocation alloc;
    VmaAllocationInfo allocationInfo;
    VkDescriptorBufferInfo descriptor;
};

///////////////////////////////////////////////////////////////////////

class DescriptorSet {
public:
    DescriptorSet() = default;
    DescriptorSet(Device& context, Shader** shaders, size_t count);
    void destroy(const Device& device);

    operator VkDescriptorSet();
    VkDescriptorSetLayout& getLayout();

    VkWriteDescriptorSet* getResource(const std::string& name);

    void update(VkDevice device);
    void update(VkDevice device, const std::string& name);

private:
    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;
    std::unordered_map<std::string, VkWriteDescriptorSet> resources;
};

} // VK
} // Raekor