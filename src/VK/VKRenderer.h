#pragma once

#include "pch.h"
#include "camera.h"
#include "VKContext.h"
#include "VKSwapchain.h"
#include "VKShader.h"
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

    bool vsync = true;

    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;

    VkCommandBuffer maincmdbuffer;
    VkCommandBuffer imguicmdbuffer;
    std::vector<VkCommandBuffer> secondaryBuffers;
    VkPipelineLayout pipelineLayout;

    std::vector<VKMesh> meshes;

    VkBuffer vertexBuffer;
    VmaAllocation vertexBufferAlloc;
    VmaAllocationInfo vertexBufferAllocInfo;

    VkBuffer indexBuffer;
    VmaAllocation indexBufferAlloc;
    VmaAllocationInfo indexBufferAllocInfo;

    // vertex input state
    VkPipelineVertexInputStateCreateInfo input_state;
    VkVertexInputBindingDescription bindingDescription;
    std::vector<VkVertexInputAttributeDescription> layout;
    
    std::unique_ptr<VK::DepthTexture> depth_texture;

    DescriptorSet shaderDescriptorSet;
    std::unique_ptr<VK::UniformBuffer> modelUbo;

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
    int current_frame = 0;

    // texture handles
    std::vector<VK::Texture> textures;
    std::vector<VkDescriptorImageInfo> textureDescriptorInfos;

    VmaAllocator bufferAllocator;

    VK::Shader vert;
    VK::Shader frag;

public:
    ~Renderer();

    void recordModel();

    void reloadShaders();

    uint32_t getMeshCount();

    Renderer(SDL_Window* window);

    void destroyGraphicsPipeline();

    void initResources();

    void setupModelStageUniformBuffers();

    void setupFrameBuffers();

    void setupSyncObjects();

    void allocateCommandBuffers();

    uint32_t getNextFrame();

    void waitForIdle();

    void recordMeshBuffer(uint32_t bufferIndex, VKMesh& m, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSets, VkCommandBuffer& cmdbuffer);

    void createGraphicsPipeline(std::array<VkPipelineShaderStageCreateInfo, 2> shaders);

    void ImGuiRecord();

    void render(uint32_t imageIndex, const glm::mat4& sky_transform);

    void recreateSwapchain(bool useVsync);

    void createAccelerationStructure();

    void ImGuiInit(SDL_Window* window);

    void ImGuiCreateFonts();

    void ImGuiNewFrame(SDL_Window* window);
};

} // vk
} // raekor
