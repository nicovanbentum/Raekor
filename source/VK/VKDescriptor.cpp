#include "pch.h"
#include "VKDescriptor.h"
#include "VKUtil.h"
#include "VKDevice.h"

namespace Raekor::VK {

void BindlessDescriptorSet::Create(const Device& device, VkDescriptorType type)
{
	const auto& descriptorIndexingProperties = device.GetPhysicalProperties().descriptorIndexingProperties;

	this->m_Type = type;
	uint32_t descriptorCount = 0;

	switch (type)
	{
		case VK_DESCRIPTOR_TYPE_SAMPLER:
		{
			descriptorCount = descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindSamplers;
			break;
		}
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		{
			descriptorCount = descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindSampledImages;
			break;
		}
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		{
			descriptorCount = descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindSampledImages;
			break;
		}
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		{
			descriptorCount = descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageImages;
			break;
		}
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		{
			descriptorCount = descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindUniformBuffers;
			break;
		}
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		{
			descriptorCount = descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageBuffers;
			break;
		}
		default:
		{
			assert(false);
			break;
		}
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

	gThrowIfFailed(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_Layout));

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

	gThrowIfFailed(vkCreateDescriptorPool(device, &pool_info, nullptr, &m_Pool));

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_Pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_Layout;
	allocInfo.pNext = &variableDescriptorCountAllocInfo;

	gThrowIfFailed(vkAllocateDescriptorSets(device, &allocInfo, &m_Set));
}



void BindlessDescriptorSet::Destroy(const Device& device)
{
	vkFreeDescriptorSets(device, m_Pool, 1, &m_Set);
	vkDestroyDescriptorSetLayout(device, m_Layout, nullptr);
	vkDestroyDescriptorPool(device, m_Pool, nullptr);
}



uint32_t BindlessDescriptorSet::Insert(const Device& device, const VkDescriptorImageInfo& imageInfo)
{
	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.descriptorCount = 1;
	write.descriptorType = m_Type;
	write.dstBinding = 0;
	write.dstSet = m_Set;
	write.pImageInfo = &imageInfo;

	if (!m_FreeIndices.empty())
	{
		write.dstArrayElement = m_FreeIndices.back();
		m_FreeIndices.pop_back();
	}
	else
		write.dstArrayElement = m_Size++;

	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

	return write.dstArrayElement;
}

} // Raekor::VK