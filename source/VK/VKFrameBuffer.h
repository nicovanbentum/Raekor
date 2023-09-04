#pragma once

namespace Raekor::VK {

class Texture;

class FrameBuffer
{
	friend class Device;

public:
	struct Desc
	{
		Desc& ColorAttachment(uint32_t index, Texture& texture);
		Desc& DepthAttachment(Texture& texture);

		uint32_t width = 0, height = 0;
		Texture* depthStencilAttachment = nullptr;
		std::array<Texture*, 8> colorAttachments = {};
	};

	uint32_t GetColorAttachmentCount() const;
	std::vector<VkFormat> GetColorAttachmentFormats() const;

	Desc description;
};

}
