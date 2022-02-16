#include "pch.h"
#include "VKFrameBuffer.h"
#include "VKTexture.h"
#include "VKDevice.h"
#include "VKUtil.h"

namespace Raekor::VK {

FrameBuffer::Desc& FrameBuffer::Desc::colorAttachment(uint32_t index, Texture& texture) {
	assert(index < colorAttachments.size());
	colorAttachments[index] = &texture;
	return *this;
}



FrameBuffer::Desc& FrameBuffer::Desc::depthAttachment(Texture& texture) {
	depthStencilAttachment = &texture;
	return *this;
}



FrameBuffer Device::createFrameBuffer(const FrameBuffer::Desc& desc) {
	FrameBuffer framebuffer = {};
	framebuffer.description = desc;
	return framebuffer;
}

void Device::destroyFrameBuffer(const FrameBuffer& frameBuffer) {
}


uint32_t FrameBuffer::GetColorAttachmentCount() const {
	uint32_t count = 0;

	for (const auto& texture : description.colorAttachments)
		if (texture)
			count++;

	return count;
}


std::vector<VkFormat> FrameBuffer::GetColorAttachmentFormats() const {
	std::vector<VkFormat> formats;

	for (const auto& texture : description.colorAttachments)
		if (texture)
			formats.push_back(texture->description.format);

	return formats;
}

} // raekor::VK