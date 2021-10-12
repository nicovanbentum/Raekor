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

	fontTextureID = textures.push_back(device, imageInfo);
	io.Fonts->TexID = &fontTextureID;

	// setup the single subpass contents
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkAttachmentDescription description = {};
	description.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	description.samples = VK_SAMPLE_COUNT_1_BIT;
	description.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	description.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
	description.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
	dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &description;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.pDependencies = &dependency;

	ThrowIfFailed(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));

	createFramebuffer(device, pathTracePass, swapchain.getExtent().width, swapchain.getExtent().height);

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

	VkVertexInputBindingDescription vertexBinding = {};
	vertexBinding.binding = 0;
	vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	vertexBinding.stride = sizeof(ImDrawVert);

	constexpr std::array vertexAttributes = {
		VkVertexInputAttributeDescription {
			0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)
		},
		VkVertexInputAttributeDescription {
			1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)
		},
		VkVertexInputAttributeDescription {
			2, 0, VK_FORMAT_R32_UINT, offsetof(ImDrawVert, col)
		}
	};


	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &vertexBinding;
	vertexInputInfo.vertexAttributeDescriptionCount = uint32_t(vertexAttributes.size());
	vertexInputInfo.pVertexAttributeDescriptions = vertexAttributes.data();

	VkPipelineViewportStateCreateInfo viewport = {};
	viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport.viewportCount = 1;
	viewport.scissorCount = 1;

	constexpr std::array dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = uint32_t(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	pixelShader = device.createShader("shaders/Vulkan/bin/imgui.frag.spv");
	vertexShader = device.createShader("shaders/Vulkan/bin/imgui.vert.spv");

	std::array stages = {
		pixelShader.getPipelineCreateInfo(),
		vertexShader.getPipelineCreateInfo()
	};

	GraphicsPipeline::State state;
	state.rasterizer.cullMode = VK_CULL_MODE_NONE;
	state.colorBlend.attachmentCount = 1;

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = uint32_t(stages.size());
	pipelineInfo.pStages = stages.data();
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.pViewportState = &viewport;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &state.inputAssembly;
	pipelineInfo.pRasterizationState = &state.rasterizer;
	pipelineInfo.pMultisampleState = &state.multisample;
	pipelineInfo.pDepthStencilState = &state.depthStencil;
	pipelineInfo.pColorBlendState = &state.colorBlend;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	ThrowIfFailed(vkCreateGraphicsPipelines(device, NULL, 1, &pipelineInfo, nullptr, &pipeline));
}


void ImGuiPass::destroy(Device& device) {
	device.destroyBuffer(indexBuffer);
	device.destroyBuffer(vertexBuffer);

	device.destroyShader(pixelShader);
	device.destroyShader(vertexShader);
	
	device.destroyTexture(fontTexture);
	device.destroySampler(fontSampler);
	
	vkDestroyRenderPass(device, renderPass, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyPipeline(device, pipeline, nullptr);
	destroyFramebuffer(device);
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
	beginInfo.renderPass = renderPass;
	beginInfo.framebuffer = framebuffer;
	beginInfo.renderArea.offset = { 0, 0 };
	beginInfo.renderArea.extent.width = width;
	beginInfo.renderArea.extent.height = height;

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };

	beginInfo.clearValueCount = (uint32_t)clearValues.size();
	beginInfo.pClearValues = clearValues.data();


	vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
	pathTracePass.finalImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

	VkDeviceSize offset[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.buffer, offset);
	
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
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
	VkFramebufferCreateInfo framebufferInfo = {};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = renderPass;
	framebufferInfo.attachmentCount = 1u;
	framebufferInfo.pAttachments = &pathTracePass.finalImageView;
	framebufferInfo.width = width;
	framebufferInfo.height = height;

	framebufferInfo.layers = 1;

	ThrowIfFailed(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer));
}



void ImGuiPass::destroyFramebuffer(Device& device) {
	vkDestroyFramebuffer(device, framebuffer, nullptr);
}

}  // Raekor::VK