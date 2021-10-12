#include "pch.h"
#include "VKFrameBuffer.h"
#include "VKTexture.h"

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

} // raekor::VK