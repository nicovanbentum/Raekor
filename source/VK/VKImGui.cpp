#include "pch.h"
#include "VKImGui.h"

#include "VKUtil.h"
#include "VKDevice.h"
#include "VKDescriptor.h"
#include "VKPass.h"
#include "VKSwapchain.h"
#include "VKPipeline.h"

namespace Raekor::VK {

void ImGuiPass::Init(Device& device, const SwapChain& swapchain, PathTracePass& pathTracePass, BindlessDescriptorSet& textures)
{
	constexpr size_t max_buffer_size = 65536; // Max number of bytes for VkCmdUpdateBuffer
	m_ScratchBuffer.resize(max_buffer_size);

	m_VertexBuffer = device.CreateBuffer(
		max_buffer_size,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY
	);

	m_IndexBuffer = device.CreateBuffer(
		max_buffer_size,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY
	);

	int width, height;
	unsigned char* pixels;
	ImGui::GetIO().Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

	Buffer stagingBuffer = device.CreateBuffer(
		width * height,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VMA_MEMORY_USAGE_CPU_ONLY
	);

	memcpy(device.GetMappedPointer<void*>(stagingBuffer), pixels, width * height);

	{
		Texture::Desc desc;
		desc.format = VK_FORMAT_R8_UNORM;
		desc.width = width;
		desc.height = height;

		m_FontTexture = device.CreateTexture(desc);
	}

	device.TransitionImageLayout(
		m_FontTexture,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	);

	device.CopyBufferToImage(stagingBuffer, m_FontTexture, width, height);

	device.TransitionImageLayout(
		m_FontTexture,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	device.DestroyBuffer(stagingBuffer);

	m_FontSampler = device.CreateSampler(Sampler::Desc());

	VkDescriptorImageInfo image_info = {};
	image_info.imageView = device.CreateView(m_FontTexture);
	image_info.sampler = m_FontSampler.sampler;
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	m_FontTextureID = textures.Insert(device, image_info);
	ImGui::GetIO().Fonts->TexID = &m_FontTextureID;

	VkPushConstantRange push_constant_range = {};
	push_constant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
	push_constant_range.size = 128; // TODO: query size from physical device

	VkPipelineLayoutCreateInfo layout_info = {};
	layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_info.setLayoutCount = 1;
	layout_info.pSetLayouts = &textures.GetLayout();
	layout_info.pushConstantRangeCount = 1;
	layout_info.pPushConstantRanges = &push_constant_range;

	gThrowIfFailed(vkCreatePipelineLayout(device, &layout_info, nullptr, &m_PipelineLayout));

	auto vertex_input = GraphicsPipeline::VertexInput()
		.Binding(0, sizeof(ImDrawVert))
		.Attribute(0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos))
		.Attribute(1, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv))
		.Attribute(2, VK_FORMAT_R32_UINT, offsetof(ImDrawVert, col));

	m_PixelShader = device.CreateShader("assets/system/shaders/Vulkan/bin/imguiPS.hlsl.spv", VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT);
	m_VertexShader = device.CreateShader("assets/system/shaders/Vulkan/bin/imguiVS.hlsl.spv", VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT);

	GraphicsPipeline::State pipeline_state;
	pipeline_state.rasterizer.cullMode = VK_CULL_MODE_NONE;
	pipeline_state.colorBlend.attachmentCount = 1;

	FrameBuffer::Desc framebuffer_desc;
	framebuffer_desc.width = swapchain.extent.width;
	framebuffer_desc.height = swapchain.extent.height;
	framebuffer_desc.ColorAttachment(0, pathTracePass.finalTexture);

	m_FrameBuffer = device.CreateFrameBuffer(framebuffer_desc);

	GraphicsPipeline::Desc pipeline_desc;
	pipeline_desc.state = &pipeline_state;
	pipeline_desc.vertexShader = &m_VertexShader;
	pipeline_desc.vertexInput = &vertex_input;
	pipeline_desc.pixelShader = &m_PixelShader;

	m_Pipeline = device.CreateGraphicsPipeline(pipeline_desc, m_FrameBuffer, m_PipelineLayout);
}


void ImGuiPass::Destroy(Device& device)
{
	device.DestroyBuffer(m_IndexBuffer);
	device.DestroyBuffer(m_VertexBuffer);

	device.DestroyShader(m_PixelShader);
	device.DestroyShader(m_VertexShader);

	device.DestroyTexture(m_FontTexture);
	device.DestroySampler(m_FontSampler);

	device.DestroyFrameBuffer(m_FrameBuffer);
	device.DestroyGraphicsPipeline(m_Pipeline);
}


void ImGuiPass::Record(Device& device, VkCommandBuffer commandBuffer, ImDrawData* data, BindlessDescriptorSet& textures, uint32_t width, uint32_t height, PathTracePass& pathTracePass)
{
	uint32_t vertex_offset = 0, index_offset = 0;

	for (int n = 0; n < data->CmdListsCount; n++)
	{
		const ImDrawList* cmdList = data->CmdLists[n];

		uint32_t vertex_size = cmdList->VtxBuffer.Size * sizeof(ImDrawVert);
		memcpy(m_ScratchBuffer.data() + vertex_offset, cmdList->VtxBuffer.Data, vertex_size);
		vertex_offset += vertex_size;
	}

	assert(vertex_offset < m_ScratchBuffer.size());

	if (vertex_offset)
		vkCmdUpdateBuffer(commandBuffer, m_VertexBuffer.buffer, 0, gAlignUp(vertex_offset, 4), m_ScratchBuffer.data());

	for (int n = 0; n < data->CmdListsCount; n++)
	{
		const ImDrawList* cmdList = data->CmdLists[n];

		uint32_t index_size = cmdList->IdxBuffer.Size * sizeof(ImDrawIdx);
		memcpy(m_ScratchBuffer.data() + index_offset, cmdList->IdxBuffer.Data, index_size);
		index_offset += index_size;
	}

	assert(index_offset < m_ScratchBuffer.size());

	if (index_offset)
		vkCmdUpdateBuffer(commandBuffer, m_IndexBuffer.buffer, 0, gAlignUp(index_offset, 4), m_ScratchBuffer.data());

	struct PushConstants
	{
		glm::mat4 projection;
		int32_t textureIndex;
	} push_constants;

	push_constants.projection = glm::ortho(0.0f, data->DisplaySize.x, data->DisplaySize.y, 0.0f);

	std::vector<VkRenderingAttachmentInfo> colorAttachments;
	colorAttachments.reserve(m_FrameBuffer.description.colorAttachments.size());

	for (const auto& texture : m_FrameBuffer.description.colorAttachments)
	{
		if (texture)
		{
			auto& attachment = colorAttachments.emplace_back();
			attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			attachment.imageView = device.CreateView(*texture);
			attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		}
	}

	VkRenderingInfo renderInfo = {};
	renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderInfo.layerCount = 1;
	renderInfo.colorAttachmentCount = 1;
	renderInfo.renderArea.extent = VkExtent2D { width, height };
	renderInfo.pColorAttachments = colorAttachments.data();
	renderInfo.colorAttachmentCount = colorAttachments.size();

	vkCmdBeginRendering(commandBuffer, &renderInfo);
	vkCmdSetDepthBiasEnable(commandBuffer, false);
	vkCmdSetPrimitiveRestartEnable(commandBuffer, false);
	vkCmdSetRasterizerDiscardEnable(commandBuffer, false);
	vkCmdSetPrimitiveTopology(commandBuffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);


	VkDeviceSize offset[] = { 0 };
	VkDeviceSize stride[] = { sizeof(ImDrawVert) };
	vkCmdBindVertexBuffers2(commandBuffer, 0, 1, &m_VertexBuffer.buffer, offset, nullptr, stride);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline.pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &textures.GetDescriptorSet(), 0, nullptr);

	index_offset = 0;
	vertex_offset = 0;

	for (uint32_t list = 0; list < uint32_t(data->CmdListsCount); list++)
	{
		const ImDrawList* cmd_list = data->CmdLists[list];

		for (const auto& cmd : cmd_list->CmdBuffer)
		{
			if (cmd.ElemCount == 0) 
				continue;

			if (cmd.UserCallback)
			{
				cmd.UserCallback(cmd_list, &cmd);
			}
			else
			{
				push_constants.textureIndex = cmd.TextureId ? *static_cast<int32_t*>( cmd.TextureId ) : -1;
				vkCmdPushConstants(commandBuffer, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &push_constants);

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
				viewport.height = -float(height);
				viewport.y = float(height);
				viewport.x = 0;

				vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
				vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

				vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer.buffer, index_offset, VK_INDEX_TYPE_UINT16);
				vkCmdDrawIndexed(commandBuffer, cmd.ElemCount, 1, 0, vertex_offset, 0);
			}

			index_offset += cmd.ElemCount * sizeof(ImDrawIdx);
		}

		vertex_offset += cmd_list->VtxBuffer.Size;
	}

	vkCmdEndRendering(commandBuffer);
}



void ImGuiPass::CreateFramebuffer(Device& device, PathTracePass& pathTracePass, uint32_t width, uint32_t height)
{
	FrameBuffer::Desc framebuffer_desc;
	framebuffer_desc.width = width;
	framebuffer_desc.height = height;
	framebuffer_desc.ColorAttachment(0, pathTracePass.finalTexture);

	m_FrameBuffer = device.CreateFrameBuffer(framebuffer_desc);
}



void ImGuiPass::DestroyFramebuffer(Device& device)
{
	device.DestroyFrameBuffer(m_FrameBuffer);
}

}  // Raekor::VK