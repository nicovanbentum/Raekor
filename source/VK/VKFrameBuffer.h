#pragma once

namespace Raekor::VK {

class Texture;

class FrameBuffer {
	friend class Device;

public:
	struct Desc {
		Desc& colorAttachment(uint32_t index, Texture& texture);
		Desc& depthAttachment(Texture& texture);
		
		uint32_t width, height;
		Texture* depthStencilAttachment;
		std::array<Texture*, 8> colorAttachments;
	};

	uint32_t GetColorAttachmentCount() const;
	std::vector<VkFormat> GetColorAttachmentFormats() const;

	Desc description;
};

}
