#pragma once

#include "pch.h"
#include "camera.h"
#include "VKContext.h"
#include "VKSwapchain.h"
#include "VKShader.h"
#include "VKTexture.h"
#include "VKDescriptor.h"
#include "VKScene.h"
#include "VKImGui.h"

namespace Raekor::VK {

class Renderer {
public:
    Renderer(SDL_Window* window);
    ~Renderer();

    void run();

    void reloadShaders();
    void setupSyncObjects();
    void recreateSwapchain(bool useVsync);
    void createAccelerationStructure();

private:
    VK::Context context;
    
    VK::GUI imgui;
    VK::Swapchain swapchain;

    bool enableValidationLayers;
    std::vector<const char*> extensions;

    bool vsync = false;

    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;

    VkCommandBuffer maincmdbuffer;

    VkPipelineLayout pipelineLayout;

    int current_frame = 0;
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

};

} // namespace
