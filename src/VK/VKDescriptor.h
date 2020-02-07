#pragma once

#include "VKBuffer.h"
#include "VKTexture.h"

namespace Raekor {
namespace VK {

class UniformBuffer : public Buffer {
public:
    UniformBuffer(const Context& ctx, size_t allocationSize, bool isDynamic);
    ~UniformBuffer();

    void update(const void* data, size_t size = VK_WHOLE_SIZE) const;
    inline size_t getSize() const { return memoryRange.size; }
    inline bool isDynamic() const { return dynamic; }
    inline const VkDescriptorBufferInfo* getDescriptor() const { return &descriptor; }

public:
    VkDescriptorBufferInfo descriptor;

private:
    bool dynamic;
    void* mappedMemory;
    VkMappedMemoryRange memoryRange;
};

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