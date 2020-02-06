#include "pch.h"
#include "VKDescriptor.h"

namespace Raekor {
namespace VK {

UniformBuffer::UniformBuffer(const Context& ctx, size_t size) 
    : Buffer(ctx.device), allocatedSize(size) 
{
    ctx.device.createBuffer(
        size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        buffer, memory);
    vkMapMemory(ctx.device, memory, 0, size, 0, &mappedMemory);

    descriptor.buffer = buffer;
    descriptor.offset = 0;
    descriptor.range = allocatedSize;
}

///////////////////////////////////////////////////////////////////////

UniformBuffer::~UniformBuffer() {
    vkUnmapMemory(device, memory);
}

///////////////////////////////////////////////////////////////////////

void UniformBuffer::update(const void* data, size_t size) const {
    memcpy(mappedMemory, data, size);
}

///////////////////////////////////////////////////////////////////////

DescriptorSet::DescriptorSet(const Context& ctx) : device(ctx.device) {}

///////////////////////////////////////////////////////////////////////

DescriptorSet::~DescriptorSet() {
    vkDestroyDescriptorSetLayout(device, layout, nullptr);
    device.freeDescriptorSet(1, &dstSet);
}

///////////////////////////////////////////////////////////////////////

void DescriptorSet::bind(uint32_t slot, const UniformBuffer& buffer, VkShaderStageFlags stages) {
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = slot;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.pImmutableSamplers = nullptr;
    binding.stageFlags = stages;
    bindings.push_back(binding);


    VkWriteDescriptorSet descriptor = {};
    descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor.dstSet = nullptr; // we allocate and reassign later
    descriptor.dstBinding = slot;
    descriptor.dstArrayElement = 0;
    descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor.descriptorCount = 1;
    descriptor.pBufferInfo = buffer.getDescriptor();
    descriptor.pImageInfo = nullptr;
    descriptor.pTexelBufferView = nullptr;
    sets.push_back(descriptor);
}

///////////////////////////////////////////////////////////////////////

void DescriptorSet::bind(uint32_t slot, const Texture& texture, VkShaderStageFlags stages) {
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
    descriptor.pImageInfo = texture.getDescriptor();
    descriptor.pTexelBufferView = nullptr;
    sets.push_back(descriptor);
}

///////////////////////////////////////////////////////////////////////

void DescriptorSet::bind(uint32_t slot, const std::vector<Texture>& textures, VkShaderStageFlags stages) {
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = slot;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = static_cast<uint32_t>(textures.size());
    binding.pImmutableSamplers = nullptr;
    binding.stageFlags = stages;
    bindings.push_back(binding);

    std::vector<const VkDescriptorImageInfo*> infos;
    for (auto& texture : textures) {
        infos.push_back(texture.getDescriptor());
    }

    VkWriteDescriptorSet descriptor = {};
    descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor.dstSet = dstSet;
    descriptor.dstBinding = slot;
    descriptor.dstArrayElement = 0;
    descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor.descriptorCount = static_cast<uint32_t>(textures.size());
    descriptor.pBufferInfo = nullptr;
    descriptor.pImageInfo = *infos.data();
    descriptor.pTexelBufferView = nullptr;
    sets.push_back(descriptor);
}

///////////////////////////////////////////////////////////////////////

void DescriptorSet::complete(const Context& ctx) {
    if (sets.empty()) {
        throw std::runtime_error("Can't complete descriptor. Did you forget to add() resources?");
    }
    // create the layout
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    for (auto& binding : bindings) {
        std::cout << " completed binding at " << binding.binding << '\n';
    }

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("fialed to create descriptor layout");
    }
    // allocate the set using the layout and reassign to the write sets
    ctx.device.allocateDescriptorSet(1, &layout, &dstSet);
    for (auto& set : sets) {
        set.dstSet = dstSet;
    }
    // update the set
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);
}

///////////////////////////////////////////////////////////////////////

VkDescriptorSetLayout DescriptorSet::getLayout() {
    if (layout == VK_NULL_HANDLE) {
        throw std::runtime_error("complete() was never called.");
    }
    return layout;
}

///////////////////////////////////////////////////////////////////////

DescriptorSet::operator VkDescriptorSet() const {
    if (dstSet != VK_NULL_HANDLE) {
        return dstSet;
    } else {
        throw std::runtime_error("complete() was never called.");
    }
}

} // VK
} // Raekor