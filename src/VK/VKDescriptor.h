#pragma once

#include "VKBuffer.h"
#include "VKTexture.h"

namespace Raekor {
namespace VK {

class UniformBuffer : public Buffer {
public:
    UniformBuffer(const Context& ctx, size_t allocationSize);
    ~UniformBuffer();

    void update(const void* data, size_t size) const;
    inline size_t getSize() const { return allocatedSize; }
    inline const VkDescriptorBufferInfo* getDescriptor() const { return &descriptor; }

private:
    void* mappedMemory;
    size_t allocatedSize;
    VkDescriptorBufferInfo descriptor;
};

class DescriptorSet {
public:
    DescriptorSet(const Context& ctx);
    ~DescriptorSet();

    void add(const Texture& texture, VkShaderStageFlags stages);
    void add(const UniformBuffer& buffer, VkShaderStageFlags stages);
    void add(const std::vector<Texture>& textures, VkShaderStageFlags stages);
    void complete(const Context& ctx);

    VkDescriptorSetLayout getLayout();
    operator VkDescriptorSet() const;

private:

private:
    const Device& device;
    VkDescriptorSet dstSet;
    VkDescriptorSetLayout layout;
    std::vector<VkWriteDescriptorSet> sets;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

} // VK
} // Raekor