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

	std::vector<VkAttachmentReference> refs;
	std::vector<VkAttachmentDescription> descriptions;

	for (const auto attachment : desc.colorAttachments) {
		if (!attachment) continue;

		VkAttachmentDescription description = {};
		description.format = attachment->description.format;
		description.samples = VK_SAMPLE_COUNT_1_BIT;
		description.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		description.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference ref = {};
		ref.attachment = uint32_t(descriptions.size());
		ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		refs.push_back(ref);
		descriptions.push_back(description);
	}

	if (desc.depthStencilAttachment) {
		VkAttachmentDescription description = {};
		description.format = desc.depthStencilAttachment->description.format;
		description.samples = VK_SAMPLE_COUNT_1_BIT;
		description.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		description.initialLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

		descriptions.push_back(description);
	}

	VkAttachmentReference depthRef = {};
	depthRef.attachment = uint32_t(descriptions.size() - 1);
	depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = uint32_t(descriptions.size());
	subpass.pColorAttachments = refs.data();

	if (desc.depthStencilAttachment) {
		subpass.pDepthStencilAttachment = &depthRef;
	}

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = uint32_t(descriptions.size());
	renderPassInfo.pAttachments = descriptions.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	ThrowIfFailed(vkCreateRenderPass(device, &renderPassInfo, nullptr, &framebuffer.renderPass));

	std::vector<VkImageView> attachments;

	for (const auto attachment : desc.colorAttachments) {
		if (attachment) {
			assert(attachment->description.width == desc.width);
			assert(attachment->description.height == desc.height);
			attachments.push_back(createView(*attachment));
		}
	}

	if (desc.depthStencilAttachment) {
		assert(desc.depthStencilAttachment->description.width == desc.width);
		assert(desc.depthStencilAttachment->description.height == desc.height);
		attachments.push_back(createView(*desc.depthStencilAttachment));
	}

	VkFramebufferCreateInfo framebufferInfo = {};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = framebuffer.renderPass;
	framebufferInfo.attachmentCount = uint32_t(descriptions.size());
	framebufferInfo.pAttachments = attachments.data();
	framebufferInfo.width = desc.width;
	framebufferInfo.height = desc.height;
	framebufferInfo.layers = 1;

	ThrowIfFailed(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer.framebuffer));

	return framebuffer;
}

void Device::destroyFrameBuffer(const FrameBuffer& frameBuffer) {
	vkDestroyRenderPass(device, frameBuffer.renderPass, nullptr);
	vkDestroyFramebuffer(device, frameBuffer.framebuffer, nullptr);
}

} // raekor::VK