#pragma once

#include "VKTexture.h"
#include "VKShader.h"

namespace Raekor::VK {

class BindlessDescriptorSet
{
public:
	void Create(const Device& device, VkDescriptorType type);
	void Destroy(const Device& device);

	uint32_t Insert(const Device& device, const VkDescriptorImageInfo& imageInfo);

	const VkDescriptorSet& GetDescriptorSet() const { return m_Set; }
	const VkDescriptorSetLayout& GetLayout() const { return m_Layout; }

private:
	uint32_t m_Size = 0;
	std::vector<uint32_t> m_FreeIndices;

	VkDescriptorSet m_Set;
	VkDescriptorPool m_Pool;
	VkDescriptorType m_Type;
	VkDescriptorSetLayout m_Layout;

};

} // Raekor::VK