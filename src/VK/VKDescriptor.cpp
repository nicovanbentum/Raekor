#include "pch.h"
#include "VKDescriptor.h"
#include "VKUtil.h"
#include "VKDevice.h"

namespace Raekor::VK {

void BindlessDescriptorSet::create(const Device& device, VkDescriptorType type) {
    const auto& descriptorIndexingProperties = device.getPhysicalProperties().descriptorIndexingProperties;

    this->type = type;
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
    bindingDesc.stageFlags = VK_SHADER_STAGE_ALL;

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



uint32_t BindlessDescriptorSet::append(const Device& device, const VkDescriptorImageInfo& imageInfo) {
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