#pragma once

#include "pch.h"
#include "scene.h"
#include "camera.h"
#include "VKPass.h"
#include "VKContext.h"
#include "VKSwapchain.h"
#include "VKShader.h"
#include "VKTexture.h"
#include "VKDescriptor.h"
#include "VKScene.h"
#include "VKImGui.h"
#include "VKAccelerationStructure.h"

namespace Raekor::VK {

struct RTGeometry {
    VkBuffer vertexBuffer;
    VmaAllocation vertexAllocation;

    VkBuffer indexBuffer;
    VmaAllocation indexAllocation;
    
    AccelerationStructure accelerationStructure;
};

class Renderer {
public:
    Renderer(SDL_Window* window);
    ~Renderer();

    void render(Scene& scene);

    void reloadShaders();
    void setupSyncObjects();
    void recreateSwapchain(bool useVsync);

    RTGeometry createBLAS(Mesh& mesh);
    AccelerationStructure createTLAS(VkAccelerationStructureInstanceKHR* instances, size_t count);

private:
    VK::Context context;
    
    VK::GUI imgui;
    VK::Swapchain swapchain;

    PathTracePass pathTracePass;

    bool enableValidationLayers;
    std::vector<const char*> extensions;

    bool vsync = false;

    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;

    int current_frame = 0;
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

    AccelerationStructure TLAS;

};

} // namespace
