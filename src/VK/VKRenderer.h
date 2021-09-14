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

struct RTMaterial {
    glm::vec4 albedo;
    glm::ivec4 textures; // x = albedo, y = normals, z = metalrough
    glm::vec4 properties; // x = metalness, y = roughness, z = emissive
};

struct Texture2 {
    VkImage image;
    VkImageView view;
    VkSampler sampler;
    VkImageLayout layout;
    VmaAllocation allocation;
};

class Renderer {
public:
    Renderer(SDL_Window* window);
    ~Renderer();

    void initialize(Scene& scene);

    void updateMaterials(Assets& assets, Scene& scene);
    void updateAccelerationStructures(Scene& scene);
    void render(const Viewport& viewport, Scene& scene);

    void setupSyncObjects();
    void recreateSwapchain(bool useVsync);

    void addMaterial(Assets& assets, const Material& material);
    int32_t addBindlessTexture(Device& device, const std::shared_ptr<TextureAsset>& asset);


    void reloadShaders();

    RTGeometry createBLAS(Mesh& mesh);
    void destroyBLAS(RTGeometry& geometry);

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

    std::vector<Texture2> textures;
    BindlessDescriptorSet bindlessTextures;

    int current_frame = 0;
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

    VkBuffer instanceBuffer;
    VmaAllocation instanceAllocation;

    VkBuffer materialBuffer;
    VmaAllocation materialAllocation;
    
    AccelerationStructure TLAS;
};

} // namespace
