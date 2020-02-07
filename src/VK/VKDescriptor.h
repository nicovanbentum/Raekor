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
    void bind(uint32_t slot, const Image* image, VkShaderStageFlags stages) {
        VkDescriptorSetLayoutBinding binding = {};
        binding.binding = slot;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.pImmutableSamplers = nullptr;
        binding.stageFlags = stages;
        bindings.push_back(binding);

        VkWriteDescriptorSet descriptor = {};
        descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor.dstSet = dstSet;
        descriptor.dstBinding = slot;
        descriptor.dstArrayElement = 0;
        descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor.descriptorCount = 1;
        descriptor.pBufferInfo = nullptr;
        descriptor.pImageInfo = &image->descriptor;
        descriptor.pTexelBufferView = nullptr;
        sets.push_back(descriptor);
    }
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