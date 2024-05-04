#pragma once

#include "VKShader.h"
#include "VKFrameBuffer.h"

namespace RK::VK {

class GraphicsPipeline
{
	friend class Device;
	friend class CommandList;

public:
	struct State
	{
		State();

		VkPipelineInputAssemblyStateCreateInfo    inputAssembly;
		VkPipelineRasterizationStateCreateInfo    rasterizer;
		VkPipelineMultisampleStateCreateInfo      multisample;
		VkPipelineDepthStencilStateCreateInfo     depthStencil;
		VkPipelineColorBlendStateCreateInfo       colorBlend;

		VkSampleMask sampleMask = 0xFFFFFFFF;
		VkPipelineColorBlendAttachmentState blendStates[8];
	};

	struct VertexInput
	{
		VertexInput& Binding(uint32_t binding, uint32_t stride);
		VertexInput& Attribute(uint32_t location, VkFormat format, uint32_t offset);

		Array<VkVertexInputBindingDescription> bindings;
		Array<VkVertexInputAttributeDescription> attributes;
	};

	struct Desc
	{
		State* state;
		Shader* vertexShader;
		Shader* pixelShader;
		VertexInput* vertexInput;
	};

	VkPipeline pipeline;
	VkPipelineLayout layout;
};

}
