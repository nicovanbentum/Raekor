#pragma once

namespace Raekor::VK {

class Device;

class SwapChain
{
public:
	operator VkSwapchainKHR() const { return m_SwapChain; }

	void Create(const Device& device, glm::uvec2 resolution, VkPresentModeKHR mode);
	void Destroy(VkDevice device);

	const VkExtent2D& GetExtent() const;
	const uint32_t GetImageCount() const;

private:
	VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;

public:
	VkExtent2D extent;
	VkFormat imageFormat;
	std::vector<VkImage> images;
	std::vector<VkCommandBuffer> submitBuffers;

};

} // Raekor