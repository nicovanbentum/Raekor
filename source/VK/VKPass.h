#pragma once

#include "VKPass.h"
#include "VKShader.h"
#include "VKStructs.h"
#include "VKDescriptor.h"

#include "Raekor/camera.h"

namespace Raekor::VK {

class SwapChain;
struct BVH;

class PathTracePass {
    friend class Renderer;

public:
    struct PushConstants {
        glm::mat4 invViewProj;
        glm::vec4 cameraPosition;
        glm::vec4 lightDir = glm::vec4(-0.1, -1, -0.2, 0.0);
        uint32_t frameCounter = 0;
        uint32_t bounces = 8;
        float sunConeAngle = 0.2f;
    };

public:
    void Init(Device& device, const SwapChain& swapchain, const BVH& bvh, const Buffer& instances, const Buffer& materials, const BindlessDescriptorSet& textures);
    void Destroy(Device& device);

    void CreateRenderTargets(Device& device, const glm::uvec2& size);
    void DestroyRenderTargets(Device& device);
    
    void CreatePipeline(Device& device);
    void CreateDescriptorSet(Device& device, const BindlessDescriptorSet& bindlessTextures);
    void CreateShaderBindingTable(Device& device);

    void UpdateDescriptorSet(Device& device, const VK::BVH& accelStruct, const Buffer& instanceBuffer, const Buffer& materialBuffer);
    void Record(const Device& context, const Viewport& viewport, VkCommandBuffer commandBuffer, const BindlessDescriptorSet& bindlessTextures);

    void ReloadShaders(Device& device);

    Texture finalTexture;
    Texture accumTexture;

private:
    VkPipeline m_Pipeline;
    VkPipelineLayout m_PipelineLayout;

    PushConstants m_PushConstants;
    VkDescriptorSet m_DescriptorSet;
    VkDescriptorSetLayout m_DescriptorSetLayout;

    Shader m_MissShader;
    Shader m_RayGenShader;
    Shader m_MissShadowShader;
    Shader m_ClosestHitShader;

    Buffer m_ShaderBindingTable;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_ShaderGroups;
};

} // raekor
