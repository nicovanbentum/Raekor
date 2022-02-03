#include "pch.h"
#include "VKImGui.h"

#include "VKUtil.h"
#include "VKDevice.h"
#include "VKDescriptor.h"
#include "VKPass.h"
#include "VKSwapchain.h"
#include "VKPipeline.h"

namespace Raekor::VK {

void ImGuiPass::initialize(Device& device, const Swapchain& swapchain, PathTracePass& pathTracePass, BindlessDescriptorSet& textures) {
	constexpr size_t maxVertices = sizeof(ImDrawVert) * 64000;
	constexpr size_t maxIndices = sizeof(ImDrawIdx) * 40000;

	vertexBuffer = device.createBuffer(
		maxVertices, 
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);

	indexBuffer = device.createBuffer(
		maxIndices, 
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);

	auto& io = ImGui::GetIO();
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);
	
	Buffer stagingBuffer = device.createBuffer(
		width * height, 
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VMA_MEMORY_USAGE_CPU_ONLY
	);

	memcpy(device.getMappedPointer(stagingBuffer), pixels, width * height);

	{
		Texture::Desc desc;
		desc.format = VK_FORMAT_R8_UNORM;
		desc.width = width;
		desc.height = height;

		fontTexture = device.createTexture(desc);
	}

	device.transitionImageLayout(
		fontTexture,
		VK_IMAGE_LAYOUT_UNDEFINED, 
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	);

	device.copyBufferToImage(stagingBuffer, fontTexture, width, height);

	device.transitionImageLayout(
		fontTexture,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	device.destroyBuffer(stagingBuffer);

	fontSampler = device.createSampler(Sampler::Desc());

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageView = device.createView(fontTexture);
	imageInfo.sampler = fontSampler.native();
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	fontTextureID = textures.append(device, imageInfo);
	io.Fonts->TexID = &fontTextureID;

	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.size = 128; // TODO: query size from physical device

	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &textures.getLayout();
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushConstantRange;

	ThrowIfFailed(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout));

	auto vertexInput = GraphicsPipeline::VertexInput()
		.binding(0, sizeof(ImDrawVert))
		.attribute(0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos))
		.attribute(1, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv))
		.attribute(2, VK_FORMAT_R32_UINT, offsetof(ImDrawVert, col));

	pixelShader = device.createShader("shaders/Vulkan/bin/imguiPS.hlsl.spv");
	vertexShader = device.createShader("shaders/Vulkan/bin/imguiVS.hlsl.spv");

	GraphicsPipeline::State state;
	state.rasterizer.cullMode = VK_CULL_MODE_NONE;
	state.colorBlend.attachmentCount = 1;

	auto framebufferDesc = FrameBuffer::Desc();
	framebufferDesc.width = swapchain.extent.width;
	framebufferDesc.height = swapchain.extent.height;
	framebufferDesc.colorAttachment(0, pathTracePass.finalTexture);

	framebuffer = device.createFrameBuffer(framebufferDesc);
	
	auto pipelineDesc = GraphicsPipeline::Desc();
	pipelineDesc.state = &state;
	pipelineDesc.vertexShader = &vertexShader;
	pipelineDesc.vertexInput = &vertexInput;
	pipelineDesc.pixelShader = &pixelShader;
	
	pipeline = device.createGraphicsPipeline(pipelineDesc, framebuffer, pipelineLayout);
}


void ImGuiPass::destroy(Device& device) {
	device.destroyBuffer(indexBuffer);
	device.destroyBuffer(vertexBuffer);

	device.destroyShader(pixelShader);
	device.destroyShader(vertexShader);
	
	device.destroyTexture(fontTexture);
	device.destroySampler(fontSampler);

	device.destroyFrameBuffer(framebuffer);
	device.destroyGraphicsPipeline(pipeline);
}



void ImGuiPass::record(Device& device, VkCommandBuffer commandBuffer, ImDrawData* data, BindlessDescriptorSet& textures, uint32_t width, uint32_t height, PathTracePass& pathTracePass) {
	uint8_t* vertexData = static_cast<uint8_t*>(device.getMappedPointer(vertexBuffer));
	uint8_t* indexData = static_cast<uint8_t*>(device.getMappedPointer(indexBuffer));
	uint32_t vtxOffset = 0, idxOffset = 0;


	for (int n = 0; n < data->CmdListsCount; n++) {
		const ImDrawList* cmdList = data->CmdLists[n];
		
		uint32_t vertexSize = cmdList->VtxBuffer.Size * sizeof(ImDrawVert);
		memcpy(vertexData + vtxOffset, cmdList->VtxBuffer.Data, vertexSize);
		vtxOffset += vertexSize;

		uint32_t indexSize = cmdList->IdxBuffer.Size * sizeof(ImDrawIdx);
		memcpy(indexData + idxOffset, cmdList->IdxBuffer.Data, indexSize);
		idxOffset += indexSize;
	}

	struct PushConstants {
		glm::mat4 projection;
		int32_t textureIndex;
	} pushConstants;

	pushConstants.projection = glm::ortho(0.0f, data->DisplaySize.x, data->DisplaySize.y, 0.0f);

	VkRenderPassBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.renderPass = framebuffer.renderPass;
	beginInfo.framebuffer = framebuffer.framebuffer;
	beginInfo.renderArea.offset = { 0, 0 };
	beginInfo.renderArea.extent.width = width;
	beginInfo.renderArea.extent.height = height;

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };

	beginInfo.clearValueCount = (uint32_t)clearValues.size();
	beginInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkDeviceSize offset[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.buffer, offset);
	
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &textures.getDescriptorSet(), 0, nullptr);

	vtxOffset = 0;
	idxOffset = 0;

	for (uint32_t list = 0; list < uint32_t(data->CmdListsCount); list++) {
		const ImDrawList* cmdList = data->CmdLists[list];

		for (const auto& cmd : cmdList->CmdBuffer) {

			if (cmd.ElemCount == 0) continue;

			if (cmd.UserCallback)
			{
				cmd.UserCallback(cmdList, &cmd);
			} else {
				pushConstants.textureIndex = cmd.TextureId ? *static_cast<int32_t*>(cmd.TextureId) : -1;
				vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

				ImVec4 clip = ImVec4(
					cmd.ClipRect.x - data->DisplayPos.x,
					cmd.ClipRect.y - data->DisplayPos.y,
					cmd.ClipRect.z - data->DisplayPos.x,
					cmd.ClipRect.w - data->DisplayPos.y
				);

				VkRect2D scissor = {};
				scissor.offset.x = int32_t(clip.x);
				scissor.offset.y = int32_t(clip.y);
				scissor.extent.width = uint32_t(clip.z - clip.x);
				scissor.extent.height = uint32_t(clip.w - clip.y);

				VkViewport viewport = {};
				viewport.maxDepth = 1.0f;
				viewport.width = float(width);
				viewport.height = - float(height);
				viewport.y = float(height);
				viewport.x = 0;

				vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
				vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
				vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, idxOffset, VK_INDEX_TYPE_UINT16);
				vkCmdDrawIndexed(commandBuffer, cmd.ElemCount, 1, 0, vtxOffset, 0);
			}

			idxOffset += cmd.ElemCount * sizeof(ImDrawIdx);
		}

		vtxOffset += cmdList->VtxBuffer.Size;
	}

	vkCmdEndRenderPass(commandBuffer);
}



void ImGuiPass::createFramebuffer(Device& device, PathTracePass& pathTracePass, uint32_t width, uint32_t height) {
	auto framebufferDesc = FrameBuffer::Desc();
	framebufferDesc.width = width;
	framebufferDesc.height = height;
	framebufferDesc.colorAttachment(0, pathTracePass.finalTexture);

	framebuffer = device.createFrameBuffer(framebufferDesc);
}



void ImGuiPass::destroyFramebuffer(Device& device) {
	device.destroyFrameBuffer(framebuffer);
}

}  // Raekor::VK