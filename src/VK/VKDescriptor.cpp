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

DescriptorSet::DescriptorSet(Context& context, Shader** shaders, size_t count) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    for (size_t i = 0; i < count; i++) {
        Shader* shader = shaders[i];

        SpvReflectShaderModule module;
        SpvReflectResult result = spvReflectCreateShaderModule(shader->spirv.size(), shader->spirv.data(), &module);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        const SpvReflectDescriptorSet* reflectSet = spvReflectGetDescriptorSet(&module, 0, &result);

        unsigned int blockIndex = 0;

        for (uint32_t i = 0; i < reflectSet->binding_count; i++) {
            const SpvReflectDescriptorBinding& reflBinding = *(reflectSet->bindings[i]);
            const VkDescriptorType type = static_cast<VkDescriptorType>(reflBinding.descriptor_type);

            VkWriteDescriptorSet descriptor = {};
            descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor.dstSet = descriptorSet;
            descriptor.dstBinding = reflBinding.binding;
            descriptor.dstArrayElement = 0;
            descriptor.descriptorType = type;
            descriptor.descriptorCount = reflBinding.count;
            descriptor.pBufferInfo = nullptr;
            descriptor.pImageInfo = nullptr;
            descriptor.pTexelBufferView = nullptr;

            this->resources[reflBinding.name] = descriptor;

            VkDescriptorSetLayoutBinding binding = {};
            binding.binding = reflBinding.binding;
            binding.descriptorType = static_cast<VkDescriptorType>(reflBinding.descriptor_type);
            binding.descriptorCount = reflBinding.count;

            for (uint32_t dim = 0; dim < reflBinding.array.dims_count; ++dim) {
                binding.descriptorCount *= reflBinding.array.dims[dim];
            }

            binding.stageFlags = static_cast<VkShaderStageFlagBits>(module.shader_stage);

            bindings.push_back(binding);
        }
    }

    std::vector<VkDescriptorBindingFlags> descriptorBindingFlags;

    for (const auto& binding : bindings) {
        if (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && binding.descriptorCount > 1) {
            descriptorBindingFlags.push_back(VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT);
        } else {
            descriptorBindingFlags.push_back(0);
        }
    }
    
    VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlags{};
    setLayoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    setLayoutBindingFlags.bindingCount = static_cast<uint32_t>(descriptorBindingFlags.size());
    setLayoutBindingFlags.pBindingFlags = descriptorBindingFlags.data();

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    for (auto& binding : bindings) {
        std::cout << " completed binding at " << binding.binding << '\n';
    }

    if (vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("fialed to create descriptor layout");
    }

    // Descriptor sets
    uint32_t variableDescCounts[] = { 20 };
    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountAllocInfo = {};
    variableDescriptorCountAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    variableDescriptorCountAllocInfo.descriptorSetCount = 1;
    variableDescriptorCountAllocInfo.pDescriptorCounts = variableDescCounts;

    context.device.allocateDescriptorSet(1, &descriptorSetLayout, &descriptorSet, &variableDescriptorCountAllocInfo);

    for (auto& resource : resources) {
        resource.second.dstSet = descriptorSet;
    }

    for (auto& res : this->resources) {
        std::cout << "Completed descriptor at " << res.second.dstBinding << std::endl;
    }
}

///////////////////////////////////////////////////////////////////////

void DescriptorSet::destroy(const Device& device) {
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    device.freeDescriptorSet(1, &descriptorSet);
}

///////////////////////////////////////////////////////////////////////

DescriptorSet::operator VkDescriptorSet() { return descriptorSet; }

///////////////////////////////////////////////////////////////////////

VkDescriptorSetLayout& DescriptorSet::getLayout() { return descriptorSetLayout; }

///////////////////////////////////////////////////////////////////////

VkWriteDescriptorSet* DescriptorSet::getResource(const std::string& name) {
    auto it = resources.find(name);
    if (it != resources.end()) {
        return &it->second;
    }
    else {
        return nullptr;
    }
}

///////////////////////////////////////////////////////////////////////

void DescriptorSet::update(VkDevice device) {
    std::vector<VkWriteDescriptorSet> writeSets;
    std::for_each(resources.begin(), resources.end(), [&](const auto& kv) {
        writeSets.push_back(kv.second);
        });

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, nullptr);
}

///////////////////////////////////////////////////////////////////////

void DescriptorSet::update(VkDevice device, const std::string& name) {
    auto resource = getResource(name);
    if (resource) {
        vkUpdateDescriptorSets(device, 1, getResource(name), 0, nullptr);
    }
}

} // VK
} // Raekor