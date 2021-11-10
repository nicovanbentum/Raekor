#pragma once

#include "VKShader.h"
#include "VKTexture.h"
#include "VKStructs.h"
#include "VKPipeline.h"
#include "VKFrameBuffer.h"

namespace Raekor::VK {

class Device;
class Swapchain;
class PathTracePass;
class BindlessDescriptorSet;

class ImGuiPass {
public:
    void initialize(Device& device, const Swapchain& swapchain, PathTracePass& pathTracePass, BindlessDescriptorSet& textures);
    void destroy(Device& device);

    void record(Device& device, VkCommandBuffer commandBuffer, ImDrawData* data, BindlessDescriptorSet& textures, uint32_t width, uint32_t height, PathTracePass& pathTracePass);
    void createFramebuffer(Device& device, PathTracePass& pathTracePass, uint32_t width, uint32_t height);
    void destroyFramebuffer(Device& device);

private:
    Shader pixelShader;
    Shader vertexShader;

    Buffer vertexBuffer;
    Buffer indexBuffer;

    Texture fontTexture;
    Sampler fontSampler;
    int32_t fontTextureID = -1;

    GraphicsPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    FrameBuffer framebuffer;

};


}
