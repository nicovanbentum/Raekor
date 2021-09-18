#include "pch.h"
#include "VKDescriptor.h"
#include "VKUtil.h"
#include "VKDevice.h"

namespace Raekor::VK {

UniformBuffer::UniformBuffer(Device& device, size_t size) {
    std::tie(buffer, alloc) = device.createBuffer(
        size, 
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    descriptor.buffer = buffer;
    descriptor.offset = 0;
    descriptor.range = VK_WHOLE_SIZE;
}



void UniformBuffer::update(VmaAllocator allocator, const void* data, size_t size) const {
    size = size == VK_WHOLE_SIZE ? allocationInfo.size : size;
    memcpy(allocationInfo.pMappedData, data, size);
    vmaFlushAllocation(allocator, alloc, 0, size);
}



void UniformBuffer::destroy(VmaAllocator allocator) {
    vmaDestroyBuffer(allocator, buffer, alloc);
}



DescriptorSet::DescriptorSet(Device& device, Shader** shaders, size_t count) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    for (uint32_t i = 0; i < count; i++) {
        Shader* shader = shaders[i];

        SpvReflectShaderModule module;
        SpvReflectResult result = spvReflectCreateShaderModule(shader->spirv.size(), shader->spirv.data(), &module);
        
        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            throw std::runtime_error("Failed to create a shader module for SPIRV-Reflect.");
        }

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
        if (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
            descriptorBindingFlags.push_back(VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
        } else {
            descriptorBindingFlags.push_back(0);
        }
    }
    
    VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlags{};
    setLayoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    setLayoutBindingFlags.bindingCount = uint32_t(descriptorBindingFlags.size());
    setLayoutBindingFlags.pBindingFlags = descriptorBindingFlags.data();

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = uint32_t(bindings.size());
    layoutInfo.pBindings = bindings.data();
    layoutInfo.pNext = &setLayoutBindingFlags;

    for (auto& binding : bindings) {
        std::cout << " completed binding at " << binding.binding << '\n';
    }

    ThrowIfFailed(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout));

    uint32_t variableDescCounts[] = { 20 };
    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountAllocInfo = {};
    variableDescriptorCountAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    variableDescriptorCountAllocInfo.descriptorSetCount = 1;
    variableDescriptorCountAllocInfo.pDescriptorCounts = variableDescCounts;

    device.allocateDescriptorSet(1, &descriptorSetLayout, &descriptorSet, &variableDescriptorCountAllocInfo);

    for (auto& resource : resources) {
        resource.second.dstSet = descriptorSet;
    }

    for (auto& res : this->resources) {
        std::cout << "Completed descriptor at " << res.second.dstBinding << std::endl;
    }
}



void DescriptorSet::destroy(const Device& device) {
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    device.freeDescriptorSet(1, &descriptorSet);
}

DescriptorSet::operator VkDescriptorSet() { 
    return descriptorSet; 
}



VkDescriptorSetLayout& DescriptorSet::getLayout() { 
    return descriptorSetLayout; 
}



VkWriteDescriptorSet* DescriptorSet::getResource(const std::string& name) {
    auto it = resources.find(name);
    if (it != resources.end()) {
        return &it->second;
    } else {
        return nullptr;
    }
}



void DescriptorSet::update(VkDevice device) {
    std::vector<VkWriteDescriptorSet> writeSets;
    
    std::for_each(resources.begin(), resources.end(), [&](const auto& kv) {
        writeSets.push_back(kv.second);
    });

    vkUpdateDescriptorSets(device, uint32_t(writeSets.size()), writeSets.data(), 0, nullptr);
}



void DescriptorSet::update(VkDevice device, const std::string& name) {
    auto resource = getResource(name);

    if (resource) {
        vkUpdateDescriptorSets(device, 1, getResource(name), 0, nullptr);
    }
}



void BindlessDescriptorSet::create(const Device& device, VkDescriptorType type, VkShaderStageFlags stages) {

    const auto& descriptorIndexingProperties = device.getPhysicalProperties().descriptorIndexingProperties;

    uint32_t descriptorCount = 0;

    switch (type) {
        case VK_DESCRIPTOR_TYPE_SAMPLER: {
            descriptorCount = descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindSamplers;
        } break;
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
            descriptorCount = descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindSampledImages;
        } break;
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {
            descriptorCount = descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindSampledImages;
        } break;
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
            descriptorCount = descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageImages;
        } break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
            descriptorCount = descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindUniformBuffers;
        } break;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
            descriptorCount = descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageBuffers;
        } break;
        default: {
            assert(false);
        } break;
    }

    assert(descriptorCount != 0);

    VkDescriptorSetLayoutBinding bindingDesc = {};
    bindingDesc.binding = 0;
    bindingDesc.descriptorType = type;
    bindingDesc.descriptorCount = descriptorCount;
    bindingDesc.stageFlags = stages;

    VkDescriptorBindingFlags bindingFlags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlags = {};
    setLayoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    setLayoutBindingFlags.bindingCount = 1u;
    setLayoutBindingFlags.pBindingFlags = &bindingFlags;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &bindingDesc;
    layoutInfo.pNext = &setLayoutBindingFlags;

    ThrowIfFailed(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout));

    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountAllocInfo = {};
    variableDescriptorCountAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    variableDescriptorCountAllocInfo.descriptorSetCount = 1;
    variableDescriptorCountAllocInfo.pDescriptorCounts = &descriptorCount;

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = type;
    poolSize.descriptorCount = descriptorCount;

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &poolSize;

    ThrowIfFailed(vkCreateDescriptorPool(device, &pool_info, nullptr, &pool));

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;
    allocInfo.pNext = &variableDescriptorCountAllocInfo;

    ThrowIfFailed(vkAllocateDescriptorSets(device, &allocInfo, &set));
}



void BindlessDescriptorSet::destroy(const Device& device) {
    vkFreeDescriptorSets(device, pool, 1, &set);
    vkDestroyDescriptorSetLayout(device, layout, nullptr);
    vkDestroyDescriptorPool(device, pool, nullptr);
}



uint32_t BindlessDescriptorSet::write(const Device& device, VkDescriptorType type, const VkDescriptorImageInfo& imageInfo) {
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.dstBinding = 0;
    write.dstSet = set;
    write.pImageInfo = &imageInfo;

    if (!freeIndices.empty()) {
        write.dstArrayElement = freeIndices.back();
        freeIndices.pop_back();
    } else {
        write.dstArrayElement = size++;
    }

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    return write.dstArrayElement;
}

} // Raekor::VK