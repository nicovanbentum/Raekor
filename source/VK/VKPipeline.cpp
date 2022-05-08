#include "pch.h"
#include "VKPipeline.h"

#include "VKDevice.h"

namespace Raekor::VK {

GraphicsPipeline::State::State() {
	inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	multisample = {};
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample.minSampleShading = 1.0f;
	multisample.pSampleMask = &sampleMask;

	depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.maxDepthBounds = 1.0;

	for (size_t i = 0; i < std::size(blendStates); i++) {
		auto& colorState = blendStates[i];
		
		colorState.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;

		colorState.blendEnable = VK_TRUE;
		colorState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorState.colorBlendOp = VK_BLEND_OP_ADD;
		colorState.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorState.alphaBlendOp = VK_BLEND_OP_ADD;
	}

	colorBlend = {};
	colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlend.logicOp = VK_LOGIC_OP_COPY;
	colorBlend.attachmentCount = uint32_t(std::size(blendStates));
	colorBlend.pAttachments = blendStates;
}



GraphicsPipeline::VertexInput& GraphicsPipeline::VertexInput::Binding(uint32_t binding, uint32_t stride) {
	bindings.push_back({ binding, stride, VK_VERTEX_INPUT_RATE_VERTEX });
	return *this;
}



GraphicsPipeline::VertexInput& GraphicsPipeline::VertexInput::Attribute(uint32_t location, VkFormat format, uint32_t offset) {
	attributes.push_back({ location, bindings.back().binding, format, offset });
	return *this;
}



GraphicsPipeline Device::CreateGraphicsPipeline(const GraphicsPipeline::Desc& desc, const FrameBuffer& framebuffer, VkPipelineLayout layout) {
	GraphicsPipeline pipeline = {};
	
	std::array stages = {
		desc.pixelShader->GetPipelineCreateInfo(),
		desc.vertexShader->GetPipelineCreateInfo()
	};

	VkPipelineViewportStateCreateInfo viewport = {};
	viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport.viewportCount = 1;
	viewport.scissorCount = 1;

	constexpr std::array dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_LINE_WIDTH,
		VK_DYNAMIC_STATE_DEPTH_BIAS,
		VK_DYNAMIC_STATE_BLEND_CONSTANTS,
		VK_DYNAMIC_STATE_DEPTH_BOUNDS,
		VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
		VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
		VK_DYNAMIC_STATE_STENCIL_REFERENCE,
		VK_DYNAMIC_STATE_CULL_MODE,
		VK_DYNAMIC_STATE_FRONT_FACE,
		VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
		VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE,
		VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
		VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
		VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
		VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE,
		VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
		VK_DYNAMIC_STATE_STENCIL_OP,
		VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE,
		VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,
		VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = uint32_t(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = uint32_t(desc.vertexInput->bindings.size());
	vertexInputInfo.pVertexBindingDescriptions = desc.vertexInput->bindings.data();
	vertexInputInfo.vertexAttributeDescriptionCount = uint32_t(desc.vertexInput->attributes.size());
	vertexInputInfo.pVertexAttributeDescriptions = desc.vertexInput->attributes.data();

	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_ALL;
	pushConstantRange.size = m_PhysicalDevice.m_Limits.maxPushConstantsSize;

	const auto& colorAttachmentFormats = framebuffer.GetColorAttachmentFormats();

	VkPipelineRenderingCreateInfo renderInfo = {};
	renderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	renderInfo.colorAttachmentCount = colorAttachmentFormats.size();
	renderInfo.pColorAttachmentFormats = colorAttachmentFormats.data();

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = uint32_t(stages.size());
	pipelineInfo.pStages = stages.data();
	pipelineInfo.layout = layout;
	pipelineInfo.pViewportState = &viewport;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &desc.state->inputAssembly;
	pipelineInfo.pRasterizationState = &desc.state->rasterizer;
	pipelineInfo.pMultisampleState = &desc.state->multisample;
	pipelineInfo.pDepthStencilState = &desc.state->depthStencil;
	pipelineInfo.pColorBlendState = &desc.state->colorBlend;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.basePipelineIndex = -1;

	pipelineInfo.pNext = &renderInfo;

	vkCreateGraphicsPipelines(m_Device, NULL, 1, &pipelineInfo, nullptr, &pipeline.pipeline);

	//TODO: ResourceInput
	pipeline.layout = layout;

	return pipeline;
}

void Device::DestroyGraphicsPipeline(const GraphicsPipeline& pipeline) {
	vkDestroyPipeline(m_Device, pipeline.pipeline, nullptr);
	vkDestroyPipelineLayout(m_Device, pipeline.layout, nullptr);
}


} // raekor