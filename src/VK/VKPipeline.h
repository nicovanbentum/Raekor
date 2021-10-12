#pragma once

#include "VKShader.h"
#include "VKFrameBuffer.h"

namespace Raekor::VK {

class GraphicsPipeline {
	friend class Device;
	friend class CommandList;

public:
	struct State {
		State();

		VkPipelineInputAssemblyStateCreateInfo    inputAssembly;
		VkPipelineRasterizationStateCreateInfo    rasterizer;
		VkPipelineMultisampleStateCreateInfo      multisample;
		VkPipelineDepthStencilStateCreateInfo     depthStencil;
		VkPipelineColorBlendStateCreateInfo       colorBlend;

		VkSampleMask sampleMask = 0xFFFFFFFF;
		VkPipelineColorBlendAttachmentState blendStates[8];
	};

	struct VertexInput {
		VertexInput& binding(uint32_t binding, uint32_t stride);
		VertexInput& attribute(uint32_t location, VkFormat format, uint32_t offset);

		std::vector<VkVertexInputBindingDescription> bindings;
		std::vector<VkVertexInputAttributeDescription> attributes;
	};

	struct Desc {
		State state = State();
		Shader* vertexShader;
		Shader* pixelShader;
		VertexInput vertexInput;
	};

private:
	VkPipeline pipeline;
	VkPipelineLayout layout;
};

}
