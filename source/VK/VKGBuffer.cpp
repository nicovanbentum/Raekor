#include "pch.h"
#include "VKGBuffer.h"
#include "VKDevice.h"
#include "VKUtil.h"
#include "VKStructs.h"
#include "Raekor/primitives.h"
#include "Raekor/components.h"

namespace Raekor::VK {

GBuffer::GBuffer(Device& device, const Viewport& viewport)
{
	vertexShader = device.CreateShader("shaders\\Vulkan\\gbuffer.vert", VK_SHADER_STAGE_VERTEX_BIT);
	pixelShader = device.CreateShader("shaders\\Vulkan\\gbuffer.frag", VK_SHADER_STAGE_FRAGMENT_BIT);

	uniformBuffer = device.CreateBuffer(
		sizeof(uniforms),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);

	CreateRenderTargets(device, viewport);
	createPipeline(device);
}



void GBuffer::createPipeline(Device& device)
{
	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.size = 128; // TODO: query size from physical device

	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushConstantRange;

	gThrowIfFailed(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout));

	auto vertexInput = GraphicsPipeline::VertexInput()
		.Binding(0, sizeof(Vertex))
		.Attribute(0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos))
		.Attribute(1, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv))
		.Attribute(2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal))
		.Attribute(3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, tangent));

	auto state = GraphicsPipeline::State();
	state.colorBlend.attachmentCount = 3;

	auto pipeDesc = GraphicsPipeline::Desc();
	pipeDesc.state = &state;
	pipeDesc.vertexShader = &vertexShader;
	pipeDesc.vertexInput = &vertexInput;
	pipeDesc.pixelShader = &pixelShader;

	pipeline = device.CreateGraphicsPipeline(pipeDesc, framebuffer, layout);
}



void GBuffer::CreateRenderTargets(Device& device, const Viewport& viewport)
{
	Texture::Desc texDesc;
	texDesc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	texDesc.width = viewport.GetSize().x;
	texDesc.height = viewport.GetSize().y;

	albedoTexture = device.CreateTexture(texDesc);
	normalTexture = device.CreateTexture(texDesc);
	materialTexture = device.CreateTexture(texDesc);

	texDesc.format = VK_FORMAT_D32_SFLOAT;
	depthTexture = device.CreateTexture(texDesc);

	auto fbDesc = FrameBuffer::Desc()
		.ColorAttachment(0, albedoTexture)
		.ColorAttachment(1, normalTexture)
		.ColorAttachment(2, materialTexture)
		.DepthAttachment(depthTexture);

	framebuffer = device.CreateFrameBuffer(fbDesc);
}



void GBuffer::DestroyRenderTargets(Device& device)
{
	device.DestroyFrameBuffer(framebuffer);
	device.DestroyTexture(albedoTexture);
	device.DestroyTexture(normalTexture);
	device.DestroyTexture(materialTexture);
	device.DestroyTexture(depthTexture);
}


void GBuffer::record(Device& device, VkCommandBuffer commandBuffer, const Scene& scene)
{
	VkRenderPassBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.renderArea.offset = { 0, 0 };
	beginInfo.renderArea.extent.width = framebuffer.description.width;
	beginInfo.renderArea.extent.height = framebuffer.description.height;

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };

	beginInfo.clearValueCount = (uint32_t)clearValues.size();
	beginInfo.pClearValues = clearValues.data();


	vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

	for (const auto& [entity, rt_geometry, transform] : scene.Each<RTGeometry, Transform>())
	{
		VkDeviceSize offset[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &rt_geometry.vertices.buffer, offset);
	}
}

}