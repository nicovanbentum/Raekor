#pragma once

#include "pch.h"
#include "camera.h"
#include "VKContext.h"
#include "VKSwapchain.h"
#include "VKShader.h"
#include "VKBuffer.h"
#include "VKTexture.h"
#include "VKDescriptor.h"

struct cb_vsDynamic {
    Raekor::MVP* mvp = nullptr;
};

namespace Raekor {
namespace VK {

struct VKMesh {
    uint32_t indexOffset, indexRange, vertexOffset;
    uint32_t textureIndex;
};

class Renderer {
public:
    cb_vsDynamic uboDynamic;
    size_t dynamicAlignment;

private:
    VK::Context context;
    VK::Swapchain swapchain;


    bool enableValidationLayers;
    std::vector<const char*> extensions;

    VkRenderPass renderPass;

    VkPipeline graphicsPipeline;
    VkPipeline skyboxPipeline;

    bool vsync = true;

    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;

    VkCommandBuffer maincmdbuffer;
    VkCommandBuffer imguicmdbuffer;
    VkCommandBuffer skyboxcmdbuffer;
    std::vector<VkCommandBuffer> secondaryBuffers;
    VkPipelineLayout pipelineLayout;
    VkPipelineLayout pipelineLayout2;

    std::vector<VKMesh> meshes;

    VulkanBuffer vertexBuffer;
    VulkanBuffer indexBuffer;

    // vertex input state
    std::vector<VkVertexInputAttributeDescription> layout;
    VkPipelineVertexInputStateCreateInfo input_state;
    VkVertexInputBindingDescription bindingDescription;
    
    std::unique_ptr<VK::DepthTexture> depth_texture;

    std::unique_ptr<VK::DescriptorSet> modelSet;
    std::unique_ptr<VK::UniformBuffer> modelUbo;

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    int current_frame = 0;

    // texture handles
    std::vector<VK::Texture> textures;

    // skybox resources
    std::unique_ptr<VK::VertexBuffer> cube_v;
    std::unique_ptr<VK::IndexBuffer> cube_i;
    std::unique_ptr<VK::CubeTexture> skybox;
    std::unique_ptr<VK::UniformBuffer> skyboxUbo;
    std::unique_ptr<VK::DescriptorSet> skyboxSet;

    VK::Shader vert;
    VK::Shader frag;
    VK::Shader skyboxv;
    VK::Shader skyboxf;

    std::array<std::string, 6> face_files;

    VmaAllocator bufferAllocator;

public:
    void recordModel();

    void reloadShaders();

    ~Renderer();

    uint32_t getMeshCount();

    Renderer(SDL_Window* window, const std::array<std::string, 6>& cubeTextureFiles);

    void cleanupSwapChain();

    void initResources();

    void setupModelStageUniformBuffers();

    void setupFrameBuffers();

    void setupSyncObjects();

    void setupSkyboxStageUniformBuffers();

    void allocateCommandBuffers();

    uint32_t getNextFrame();

    void waitForIdle();

    void recordSkyboxBuffer(VK::VertexBuffer* cubeVertices, VK::IndexBuffer* cubeIndices, VkPipelineLayout pLayout, VkDescriptorSet set, VkCommandBuffer& cmdbuffer);

    void recordMeshBuffer(uint32_t bufferIndex, VKMesh& m, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSets, VkCommandBuffer& cmdbuffer);

    void createGraphicsPipeline(std::array<VkPipelineShaderStageCreateInfo, 2> shaders, std::array<VkPipelineShaderStageCreateInfo, 2> skyboxShaders);

    void ImGuiRecord();

    void render(uint32_t imageIndex, const glm::mat4& sky_transform);

    void recreateSwapchain(bool useVsync);

    void ImGuiInit(SDL_Window* window);

    void ImGuiCreateFonts();

    void ImGuiNewFrame(SDL_Window* window);
};

} // vk
} // raekor
