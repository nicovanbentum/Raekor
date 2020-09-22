#pragma once

#include "VKTexture.h"

namespace Raekor {
namespace VK {

class UniformBuffer {
public:
    UniformBuffer(const Context& ctx, VmaAllocator allocator, size_t allocationSize);

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
    DescriptorSet(const Context& ctx);
    ~DescriptorSet();

    void bind(uint32_t slot, const Texture& texture, VkShaderStageFlags stages);
    void bind(uint32_t slot, const UniformBuffer& buffer, VkShaderStageFlags stages);
    void bind(uint32_t slot, const Image* image, VkShaderStageFlags stages);
    void bind(uint32_t slot, const std::vector<Texture>& textures, VkShaderStageFlags stages);
    void complete(const Context& ctx);

    operator VkDescriptorSet() const;

public:
    VkDescriptorSetLayout layout;

private:
    const Device& device;
    VkDescriptorSet dstSet;
    std::vector<VkWriteDescriptorSet> sets;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

} // VK
} // Raekor