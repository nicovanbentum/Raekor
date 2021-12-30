#pragma once

#include "VKPass.h"
#include "VKImGui.h"
#include "VKDevice.h"
#include "VKSwapchain.h"
#include "VKDescriptor.h"
#include "VKAccelerationStructure.h"

#include "scene.h"
#include "camera.h"

namespace Raekor::VK {

class Renderer {
public:
    Renderer(SDL_Window* window);
    ~Renderer();

    void initialize(Scene& scene);

    void resetAccumulation();

    void updateMaterials(Assets& assets, Scene& scene);
    void updateAccelerationStructures(Scene& scene);
    void render(SDL_Window* window, const Viewport& viewport, Scene& scene);

    void screenshot(const std::string& filepath);

    void setupSyncObjects();
    void recreateSwapchain(SDL_Window* window);

    int32_t addBindlessTexture(Device& device, const TextureAsset::Ptr& asset, VkFormat format);

    void reloadShaders();
    void setVsync(bool on);

    PathTracePass::PushConstants& constants() { return pathTracePass.pushConstants; }

    RTGeometry createBLAS(Mesh& mesh);
    void destroyBLAS(RTGeometry& geometry);

    AccelerationStructure createTLAS(VkAccelerationStructureInstanceKHR* instances, size_t count);

private:
    Device device;
    
    Swapchain swapchain;

    PathTracePass pathTracePass;
    ImGuiPass imGuiPass;

    bool enableValidationLayers;
    std::vector<const char*> extensions;

    bool vsync = false;

    std::vector<VkFence> commandsFinishedFences;
    std::vector<VkSemaphore> imageAcquiredSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;

    std::vector<Texture> textures;
    std::vector<Sampler> samplers;
    BindlessDescriptorSet bindlessTextures;

    uint32_t imageIndex = 0;
    uint32_t currentFrame = 0;
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

    Buffer instanceBuffer;
    Buffer materialBuffer;
    AccelerationStructure TLAS;
};

} // Raekor::VK
