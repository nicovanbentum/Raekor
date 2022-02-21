#pragma once

#include "VKPass.h"
#include "VKImGui.h"
#include "VKDevice.h"
#include "VKSwapchain.h"
#include "VKDescriptor.h"

#include "Raekor/scene.h"
#include "Raekor/camera.h"

namespace Raekor::VK {

class Renderer {
public:
    Renderer(SDL_Window* window);
    ~Renderer();

    void Init(Scene& scene);
    void ResetAccumulation();

    void UpdateBVH(Scene& scene);
    void UpdateMaterials(Assets& assets, Scene& scene);

    void RenderScene(SDL_Window* window, const Viewport& viewport, Scene& scene);

    void Screenshot(const std::string& path);
    void RecreateSwapchain(SDL_Window* window);

    int32_t UploadTexture(Device& device, const TextureAsset::Ptr& asset, VkFormat format);

    void ReloadShaders();
    void SetSyncInterval(bool interval);

    RTGeometry CreateBottomLevelBVH(Mesh& mesh);
    void DestroyBottomLevelBVH(RTGeometry& geometry);
    BVH CreateTopLevelBVH(VkAccelerationStructureInstanceKHR* instances, size_t count);
    
    PathTracePass::PushConstants& GetPushConstants() { return m_PathTracePass.m_PushConstants; }

private:
    bool m_SyncInterval = false;

    Device m_Device;
    SwapChain m_Swapchain;

    ImGuiPass m_ImGuiPass;
    PathTracePass m_PathTracePass;

    std::vector<VkFence> m_CommandsFinishedFences;
    std::vector<VkSemaphore> m_ImageAcquiredSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;

    std::vector<Texture> m_Textures;
    std::vector<Sampler> m_Samplers;
    BindlessDescriptorSet m_BindlessTextureSet;

    uint32_t m_ImageIndex = 0;
    uint32_t m_CurrentFrame = 0;
    static constexpr uint32_t sMaxFramesInFlight = 3;

    BVH m_TopLevelBVH;
    Buffer m_InstanceBuffer;
    Buffer m_MaterialBuffer;
};

} // Raekor::VK
