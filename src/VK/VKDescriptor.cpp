#include "pch.h"
#include "VKDescriptor.h"

namespace Raekor {
namespace VK {

UniformBuffer::UniformBuffer(const Context& ctx, VmaAllocator allocator, size_t size) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    auto vkresult = vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &buffer, &alloc, &allocationInfo);
    assert(vkresult == VK_SUCCESS);

    descriptor.buffer = buffer;
    descriptor.offset = 0;
    descriptor.range = VK_WHOLE_SIZE;
}

///////////////////////////////////////////////////////////////////////

void UniformBuffer::update(VmaAllocator allocator, const void* data, size_t size) const {
    size = size == VK_WHOLE_SIZE ? allocationInfo.size : size;
    memcpy(allocationInfo.pMappedData, data, size);
    vmaFlushAllocation(allocator, alloc, 0, size);
}

///////////////////////////////////////////////////////////////////////

void UniformBuffer::destroy(VmaAllocator allocator) {
    vmaDestroyBuffer(allocator, buffer, alloc);
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
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount = 1;
    binding.pImmutableSamplers = nullptr;
    binding.stageFlags = stages;
    bindings.push_back(binding);


    VkWriteDescriptorSet descriptor = {};
    descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor.dstSet = nullptr; // we allocate and reassign later
    descriptor.dstBinding = slot;
    descriptor.dstArrayElement = 0;
    descriptor.descriptorType = binding.descriptorType;
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
    descriptor.pImageInfo = &texture.descriptor;
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

    // TODO: ugly hack to make it so the image infos persist till complete
    VkDescriptorImageInfo* infos = new VkDescriptorImageInfo[textures.size()];
    for (size_t i = 0; i < textures.size(); i++) {
        infos[i] = textures[i].descriptor;
    }

    VkWriteDescriptorSet descriptor = {};
    descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor.dstSet = dstSet;
    descriptor.dstBinding = slot;
    descriptor.dstArrayElement = 0;
    descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor.descriptorCount = static_cast<uint32_t>(textures.size());
    descriptor.pBufferInfo = nullptr;
    descriptor.pImageInfo = infos;
    descriptor.pTexelBufferView = nullptr;
    sets.push_back(descriptor);
}

///////////////////////////////////////////////////////////////////////

void DescriptorSet::bind(uint32_t slot, const Image* image, VkShaderStageFlags stages) {
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

    for (const auto& set : sets) {
        if (set.pImageInfo != nullptr) {
            //operator delete[]((void*)set.pImageInfo);
        }
    }
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