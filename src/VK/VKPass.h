#pragma once

#include "VKPass.h"
#include "VKShader.h"
#include "VKDescriptor.h"
#include "camera.h"

namespace Raekor::VK {

class Raekor::Viewport;
class Swapchain;
struct AccelerationStructure;

class PathTracePass {
public:
    struct PushConstants {
        glm::mat4 invViewProj;
        glm::vec4 cameraPosition;
        uint32_t frameCounter = 0;
    } pushConstants;

public:
    void initialize(Context& context, const Swapchain& swapchain, const AccelerationStructure& accelStruct, VkBuffer instanceBuffer, VkBuffer materialBuffer, const BindlessDescriptorSet& bindlessTextures);
    void destroy(Device& device);

    void createRenderTextures(Device& device, const glm::uvec2& size);
    void createPipeline(Device& device, uint32_t maxRecursionDepth);
    void createDescriptorSet(Device& device, const BindlessDescriptorSet& bindlessTextures);
    void createShaderBindingTable(Device& device, PhysicalDevice& physicalDevice);

    void updateDescriptorSet(Device& device, const VK::AccelerationStructure& accelStruct, VkBuffer instanceBuffer, VkBuffer materialBuffer);
    void recordCommands(const Context& context, const Viewport& viewport, VkCommandBuffer commandBuffer, const BindlessDescriptorSet& bindlessTextures);

    void reloadShadersFromDisk(Device& device);

    void destroyRenderTextures(Device& device);

    VkImage finalImage;
    VkImageLayout finalImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage accumImage;
    VkImageLayout accumImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;


private:
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    VkImageView finalImageView;
    VmaAllocation finalImageAllocation;

    VkImageView accumImageView;
    VmaAllocation accumImageAllocation;

    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout; // TODO: move this somewhere else

    Shader rgenShader;
    Shader rmissShader;
    Shader rchitShader;
    Shader rmissShadowShader;

    VkBuffer shaderBindingTableBuffer;
    VmaAllocation shaderBindingTableAllocation;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;
};

} // raekor
