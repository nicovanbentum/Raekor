#pragma once

#include "Raekor/scene.h"
#include "Raekor/camera.h"
#include "VKShader.h"
#include "VKTexture.h"
#include "VKStructs.h"
#include "VKFrameBuffer.h"
#include "VKPipeline.h"

namespace RK::VK {

class GBuffer
{
	struct
	{
		glm::mat4 prevViewProj;
		glm::mat4 projection;
		glm::mat4 view;
		glm::mat4 model;
		glm::vec4 colour;
		glm::vec2 jitter;
		glm::vec2 prevJitter;
		float metallic;
		float roughness;
		uint32_t entity;
	} uniforms;

public:
	GBuffer(Device& device, const Viewport& viewport);
	~GBuffer();

	void record(Device& device, VkCommandBuffer commandBuffer, const Scene& scene);

	void createPipeline(Device& device);
	void CreateRenderTargets(Device& device, const Viewport& viewport);
	void DestroyRenderTargets(Device& device);

public:
	FrameBuffer framebuffer;
	Texture depthTexture;
	Texture albedoTexture;
	Texture normalTexture;
	Texture materialTexture;

private:
	Shader pixelShader;
	Shader vertexShader;
	Buffer uniformBuffer;

	VkPipelineLayout layout;
	GraphicsPipeline pipeline;
};

}
