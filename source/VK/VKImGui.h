#pragma once

#include "VKShader.h"
#include "VKTexture.h"
#include "VKStructs.h"
#include "VKPipeline.h"
#include "VKFrameBuffer.h"

namespace Raekor::VK {

class Device;
class SwapChain;
class PathTracePass;
class BindlessDescriptorSet;

class ImGuiPass
{
public:
	void Init(Device& device, const SwapChain& swapchain, PathTracePass& pathTracePass, BindlessDescriptorSet& textures);
	void Destroy(Device& device);

	void Record(Device& device, VkCommandBuffer commandBuffer, ImDrawData* data, BindlessDescriptorSet& textures, uint32_t width, uint32_t height, PathTracePass& pathTracePass);
	void CreateFramebuffer(Device& device, PathTracePass& pathTracePass, uint32_t width, uint32_t height);
	void DestroyFramebuffer(Device& device);

private:
	Shader m_PixelShader;
	Shader m_VertexShader;

	Buffer m_IndexBuffer;
	Buffer m_VertexBuffer;
	std::vector<uint8_t> m_ScratchBuffer;

	Texture m_FontTexture;
	Sampler m_FontSampler;
	int32_t m_FontTextureID = -1;

	FrameBuffer m_FrameBuffer;
	GraphicsPipeline m_Pipeline;
	VkPipelineLayout m_PipelineLayout;


};


}
