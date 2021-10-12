#pragma once

#include "VKPass.h"
#include "VKShader.h"
#include "VKStructs.h"
#include "VKDescriptor.h"

#include "camera.h"

namespace Raekor::VK {

class Swapchain;
struct AccelerationStructure;

class PathTracePass {
public:
    struct PushConstants {
        glm::mat4 invViewProj;
        glm::vec4 cameraPosition;
        glm::vec4 lightDir = glm::vec4(-0.1, -1, -0.2, 0.0);
        uint32_t frameCounter = 0;
        uint32_t bounces = 1;
        float sunConeAngle = 1.0f;
    } pushConstants;

public:
    void initialize(Device& device, const Swapchain& swapchain, const AccelerationStructure& accelStruct, const Buffer& instanceBuffer, const Buffer& materialBuffer, const BindlessDescriptorSet& bindlessTextures);
    void destroy(Device& device);

    void createRenderTextures(Device& device, const glm::uvec2& size);
    void createPipeline(Device& device, uint32_t maxRecursionDepth);
    void createDescriptorSet(Device& device, const BindlessDescriptorSet& bindlessTextures);
    void createShaderBindingTable(Device& device);

    void updateDescriptorSet(Device& device, const VK::AccelerationStructure& accelStruct, const Buffer& instanceBuffer, const Buffer& materialBuffer);
    void recordCommands(const Device& context, const Viewport& viewport, VkCommandBuffer commandBuffer, const BindlessDescriptorSet& bindlessTextures);

    void reloadShadersFromDisk(Device& device);

    void destroyRenderTextures(Device& device);

    VkImage finalImage;
    VkImageView finalImageView;
    VkImageLayout finalImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage accumImage;
    VkImageLayout accumImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;


private:
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    VmaAllocation finalImageAllocation;

    VkImageView accumImageView;
    VmaAllocation accumImageAllocation;

    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout; // TODO: move this somewhere else

    Shader rgenShader;
    Shader rmissShader;
    Shader rchitShader;
    Shader rmissShadowShader;

    Buffer shaderBindingTable;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;
};

} // raekor
