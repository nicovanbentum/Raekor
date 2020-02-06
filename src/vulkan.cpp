#include "pch.h"
#include "app.h"
#include "mesh.h"
#include "util.h"
#include "timer.h"
#include "entry.h"
#include "camera.h"
#include "buffer.h"
#include "PlatformContext.h"
#include "vulkan/vulkan.hpp"
#include "VK/VKContext.h"
#include "VK/VKSwapchain.h"
#include "VK/VKShader.h"
#include "VK/VKBuffer.h"

#define _glslc "dependencies\\glslc.exe "
#define _cl(in, out) system(std::string(_glslc + static_cast<std::string>(in) + static_cast<std::string>(" -o ") + static_cast<std::string>(out)).c_str())
void compile_shader(const char* in, const char* out) {
    if (_cl(in, out) != 0) {
        std::cout << "failed to compile vulkan shader: " + std::string(in) << '\n';
    }
    else {
        std::cout << "Successfully compiled VK shader: " + std::string(in) << '\n';

    }
}

struct skyboxmvp {
    glm::mat4 MVP;
};

namespace Raekor {

struct stbData {
    unsigned char* pixels;
    int w, h, ch;
};

struct cb_vsDynamic {
    Raekor::MVP* mvp = nullptr;
} uboDynamic;
size_t dynamicAlignment;

struct mod {
    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 position = {0.0, 0.0f, 0.0f}, scale = { 1.0f, 1.0f, 1.0f }, rotation = { 0, 0, 0 };

    void transform() {
        model = glm::mat4(1.0f);
        model = glm::translate(model, position);
        auto rotation_quat = static_cast<glm::quat>(rotation);
        model = model * glm::toMat4(rotation_quat);
        model = glm::scale(model, scale);
    }
};

struct queue_indices {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;

    bool isComplete() {
        return graphics.has_value() and present.has_value();
    }
};

class VKTexture {
public:
    VKTexture() {}

    VKTexture(VkImage p_image, VkDeviceMemory p_memory, VkImageView p_view, VkSampler p_sampler) :
        image(p_image), memory(p_memory), view(p_view), sampler(p_sampler) {}

    VKTexture(VkImage p_image, VkDeviceMemory p_memory, VkImageView p_view) :
        image(p_image), memory(p_memory), view(p_view), sampler(VK_NULL_HANDLE) {}

    inline VkImage getImage() const { return image; }
    inline VkSampler getSampler() const { return sampler; }
    inline VkImageView getView() const { return view; }
    inline VkDeviceMemory getMemory() const { return memory; }

private:
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkSampler sampler;
};

static std::array < std::string, 6> face_files;

struct VKMesh {
    uint32_t indexOffset, indexRange, vertexOffset;
    uint32_t textureIndex;
};

class VKRender {
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
    std::unique_ptr<VK::VertexBuffer> meshVertexBuffer;
    std::unique_ptr<VK::IndexBuffer> meshIndexBuffer;
    

    VkPipelineVertexInputStateCreateInfo input_state;
    VKTexture depth_texture;

    VkBuffer uniformBuffer;
    VkBuffer uniformBuffer2;
    VkDeviceMemory uniformBuffersMemory;
    VkDeviceMemory uniformBuffersMemory2;
    VkDescriptorSet descriptorSet;
    VkDescriptorSet descriptorSet2;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSetLayout descriptorSetLayout2;

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    int current_frame = 0;

    skyboxmvp uniformBufferObject;
    void* mapped;
    size_t bufferSize;

    // texture handles
    std::vector<VKTexture> textures;

    // skybox resources
    std::unique_ptr<VK::VertexBuffer> cube_v;
    std::unique_ptr<VK::IndexBuffer> cube_i;
    VKTexture skybox;

    VK::Shader vert;
    VK::Shader frag;
    VK::Shader skyboxv;
    VK::Shader skyboxf;

public:
    void recordModel() {
        // pre-record the entire model's mesh buffers
        // hardcoded for correct depth ordering, TODO: implement actual depth ordering for transparency
        for (int i = 0; i < meshes.size(); i++) {
            recordMeshBuffer(i, meshes[i], pipelineLayout, descriptorSet, secondaryBuffers[meshes.size() - 1 - i]);
        }
        recordMeshBuffer(20, meshes[20], pipelineLayout, descriptorSet, secondaryBuffers[20]);
        int other_index = (int)meshes.size() - 1 - 20;
        recordMeshBuffer(other_index, meshes[other_index], pipelineLayout, descriptorSet, secondaryBuffers[other_index]);
    }

    void reloadShaders() {
        // wait for the device to idle , since it's an engine function we dont care about performance
        if (vkDeviceWaitIdle(context.device) != VK_SUCCESS) {
            throw std::runtime_error("failed to wait for the gpu to idle");
        }
        // recompile and reload the shaders
        compile_shader("shaders/Vulkan/vulkan.vert", "shaders/Vulkan/vert.spv");
        compile_shader("shaders/Vulkan/vulkan.frag", "shaders/Vulkan/frag.spv");
        vert.reload();
        frag.reload();
        std::array<VkPipelineShaderStageCreateInfo, 2> shaders = {
            vert.getInfo(VK_SHADER_STAGE_VERTEX_BIT),
            frag.getInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
        };

        std::array<VkPipelineShaderStageCreateInfo, 2> shaders2 = {
            skyboxv.getInfo(VK_SHADER_STAGE_VERTEX_BIT),
            skyboxf.getInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
        };
        // recreate the graphics pipeline and re-record the mesh buffers
        // TODO: this still uses the depth order hack
        createGraphicsPipeline(shaders, shaders2);
        recordModel();
    }

    ~VKRender() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        // destroy/free descriptor stuff
        vkFreeMemory(context.device, uniformBuffersMemory, nullptr);
        vkFreeMemory(context.device, uniformBuffersMemory2, nullptr);
        vkDestroyBuffer(context.device, uniformBuffer, nullptr);
        vkDestroyBuffer(context.device, uniformBuffer2, nullptr);
        vkFreeDescriptorSets(context.device, context.device.descriptorPool, 1, &descriptorSet);
        vkFreeDescriptorSets(context.device, context.device.descriptorPool, 1, &descriptorSet2);
        vkDestroyDescriptorSetLayout(context.device, descriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(context.device, descriptorSetLayout2, nullptr);

        // destroy/free pipeline related stuff
        vkDestroyPipeline(context.device, graphicsPipeline, nullptr);
        vkDestroyPipeline(context.device, skyboxPipeline, nullptr);
        vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);
        vkDestroyPipelineLayout(context.device, pipelineLayout2, nullptr);

        // destroy/free texture resources
        for (VKTexture& texture : textures) {
            vkDestroyImage(context.device, texture.getImage(), nullptr);
            vkDestroyImageView(context.device, texture.getView(), nullptr);
            vkDestroySampler(context.device, texture.getSampler(), nullptr);
            vkFreeMemory(context.device, texture.getMemory(), nullptr);
        }
        vkDestroyImage(context.device, skybox.getImage(), nullptr);
        vkDestroyImageView(context.device, skybox.getView(), nullptr);
        vkDestroySampler(context.device, skybox.getSampler(), nullptr);
        vkFreeMemory(context.device, skybox.getMemory(), nullptr);

        vkDestroyImage(context.device, depth_texture.getImage(), nullptr);
        vkDestroyImageView(context.device, depth_texture.getView(), nullptr);
        vkDestroySampler(context.device, depth_texture.getSampler(), nullptr);
        vkFreeMemory(context.device, depth_texture.getMemory(), nullptr);

        // destroy/free command buffer stuff
        std::array<VkCommandBuffer, 3> buffers = { maincmdbuffer, imguicmdbuffer, skyboxcmdbuffer };
        vkFreeCommandBuffers(context.device, context.device.commandPool, (uint32_t)buffers.size(), buffers.data());
        vkFreeCommandBuffers(context.device, context.device.commandPool, (uint32_t)secondaryBuffers.size(), secondaryBuffers.data());

        // destroy/free sync objects
        for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(context.device, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(context.device, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(context.device, inFlightFences[i], nullptr);
        }

        // destroy render pass
        vkDestroyRenderPass(context.device, renderPass, nullptr);
    }
    uint32_t getMeshCount() {
        return static_cast<uint32_t>(meshes.size());
    }

    VKRender(SDL_Window* window)
        :   context(window),
            vert(context, "shaders/Vulkan/vert.spv"),
            frag(context, "shaders/Vulkan/frag.spv"),
            skyboxf(context, "shaders/Vulkan/f_skybox.spv"),
            skyboxv(context, "shaders/Vulkan/v_skybox.spv")
    {
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        VkPresentModeKHR mode = vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
        swapchain.recreate(context, { w, h }, mode);

        initResources();
        setupModelStageUniformBuffers();
        setupSkyboxStageUniformBuffers();

        std::array<VkPipelineShaderStageCreateInfo, 2> shaders = {
            vert.getInfo(VK_SHADER_STAGE_VERTEX_BIT),
            frag.getInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
        };

        std::array<VkPipelineShaderStageCreateInfo, 2> shaders2 = {
            skyboxv.getInfo(VK_SHADER_STAGE_VERTEX_BIT),
            skyboxf.getInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
        };

        // create the graphics pipeline
        createGraphicsPipeline(shaders, shaders2);
        setupFrameBuffers();

        // allocate and record the command buffers for imgui, skybox and scene
        allocateCommandBuffers();
        recordModel();
        recordSkyboxBuffer(cube_v.get(), cube_i.get(), pipelineLayout2, descriptorSet2, skyboxcmdbuffer);
        
        // setup sysnc objects for synchronization between cpu and gpu
        setupSyncObjects();
    }

    void cleanupSwapChain() {
        vkDestroyPipeline(context.device, graphicsPipeline, nullptr);
        vkDestroyPipeline(context.device, skyboxPipeline, nullptr);
        vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);
        vkDestroyPipelineLayout(context.device, pipelineLayout2, nullptr);
        vkDestroyRenderPass(context.device, renderPass, nullptr);

        vkDestroyImage(context.device, depth_texture.getImage(), nullptr);
        vkDestroyImageView(context.device, depth_texture.getView(), nullptr);
        vkDestroySampler(context.device, depth_texture.getSampler(), nullptr);
        vkFreeMemory(context.device, depth_texture.getMemory(), nullptr);
    }

    void initResources() {
        VkPipelineShaderStageCreateInfo shaderStages[] = {
            vert.getInfo(VK_SHADER_STAGE_VERTEX_BIT),
            frag.getInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
        };

        VkPipelineShaderStageCreateInfo skyboxShaders[]{
            skyboxv.getInfo(VK_SHADER_STAGE_VERTEX_BIT),
            skyboxf.getInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
        };

        std::vector<std::vector<Index>> indexbuffers;

        constexpr unsigned int flags =
            aiProcess_CalcTangentSpace |
            aiProcess_Triangulate |
            aiProcess_SortByPType |
            aiProcess_PreTransformVertices |
            aiProcess_JoinIdenticalVertices |
            aiProcess_GenUVCoords |
            aiProcess_OptimizeMeshes |
            aiProcess_Debone |
            aiProcess_ValidateDataStructure;

        Assimp::Importer importer;
        std::string path = "resources/models/Sponza/Sponza.gltf";
        const auto scene = importer.ReadFile(path, flags);
        m_assert(scene && scene->HasMeshes(), "failed to load mesh");

        std::map<std::string, int> seen;

        meshes.reserve(scene->mNumMeshes);
        std::vector<Vertex> vertices;
        std::vector<Index> indices;

        for (unsigned int m = 0, ti = 0; m < scene->mNumMeshes; m++) {
            auto ai_mesh = scene->mMeshes[m];

            VKMesh mm;
            mm.indexOffset = indices.size();
            mm.indexRange = ai_mesh->mNumFaces * 3;
            mm.vertexOffset = vertices.size();
            meshes.push_back(mm);

            for (size_t i = 0; i < ai_mesh->mNumVertices; i++) {
                Vertex v;
                v.pos = { ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z };
                if (ai_mesh->HasTextureCoords(0)) {
                    v.uv = { ai_mesh->mTextureCoords[0][i].x, ai_mesh->mTextureCoords[0][i].y };
                }
                if (ai_mesh->HasNormals()) {
                    v.normal = { ai_mesh->mNormals[i].x, ai_mesh->mNormals[i].y, ai_mesh->mNormals[i].z };
                }
                vertices.push_back(std::move(v));
            }
            // extract indices
            for (size_t i = 0; i < ai_mesh->mNumFaces; i++) {
                m_assert((ai_mesh->mFaces[i].mNumIndices == 3), "faces require 3 indices");
                indices.push_back({ ai_mesh->mFaces[i].mIndices[0], ai_mesh->mFaces[i].mIndices[1], ai_mesh->mFaces[i].mIndices[2] });
            }

        }

        meshIndexBuffer = std::make_unique<VK::IndexBuffer>(context, indices);
        meshVertexBuffer = std::make_unique<VK::VertexBuffer>(context, vertices);
        meshVertexBuffer->setLayout({
            { "POSITION", ShaderType::FLOAT3 },
            { "UV",       ShaderType::FLOAT2 },
            { "NORMAL",   ShaderType::FLOAT3 }
            });

        for (unsigned int m = 0, ti = 0; m < scene->mNumMeshes; m++) {
            auto ai_mesh = scene->mMeshes[m];

            std::string texture_path;
            auto material = scene->mMaterials[ai_mesh->mMaterialIndex];
            if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
                aiString filename;
                material->GetTexture(aiTextureType_DIFFUSE, 0, &filename);
                texture_path = get_file(path, PATH_OPTIONS::DIR) + std::string(filename.C_Str());
            }

            if (!texture_path.empty()) {
                if (seen.find(texture_path) == seen.end()) {
                    meshes[m].textureIndex = ti;;
                    seen[texture_path] = ti;
                    ti++;
                }
                else meshes[m].textureIndex = seen[texture_path];
            }
            else meshes[m].textureIndex = -1;
        }

        static std::mutex mutex;

        std::vector<std::string> texture_paths = std::vector<std::string>(seen.size());
        for (auto& kv : seen) {
            texture_paths[kv.second] = kv.first;
        }

        std::vector<stbData> stb_images;
        stb_images.resize(seen.size());
        auto load_image = [&](std::pair<std::string, int> kv) {
            stbi_set_flip_vertically_on_load(true);
            stbData stb = {};
            // we shouldnt have to flip texture for vulkan, TODO: figure out why everything is upside down
            stb.pixels = stbi_load(kv.first.c_str(), &stb.w, &stb.h, &stb.ch, STBI_rgb_alpha);
            if (!stb.pixels) {
                throw std::runtime_error("failed to load stb image");
            }
            std::lock_guard<std::mutex> lock(mutex);
            stb_images[kv.second] = stb;
        };

        std::vector<std::future<void>> futures;
        for (const auto& kv : seen) {
            futures.push_back(std::async(std::launch::async, load_image, kv));
        }

        for (auto& future : futures) {
            future.wait();
        }

        for (const stbData& image : stb_images) {
            textures.push_back(loadTexture(image));
        }

        // skybox resources, load_skybox basically uploads a cubemap image
        // cube v and i are the vertex and index buffer for a cube
        skybox = load_skybox();
        cube_v = std::make_unique<VK::VertexBuffer>(context, v_cube);
        cube_i = std::make_unique<VK::IndexBuffer>(context, i_cube);
        
        input_state = meshVertexBuffer->getState();

        std::cout << "mesh total = " << meshes.size() << "\n";
    }

    void setupModelStageUniformBuffers() {
        VkDescriptorSetLayoutBinding uboLayoutBinding = {};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.pImmutableSamplers = nullptr; // Optional
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorCount = static_cast<uint32_t>(textures.size());
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("fialed to create descriptor layout");
        }

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(context.PDevice, &props);
        // Calculate required alignment based on minimum device offset alignment
        size_t minUboAlignment = props.limits.minUniformBufferOffsetAlignment;
        dynamicAlignment = sizeof(MVP);
        
        if (minUboAlignment > 0) {
            dynamicAlignment = (dynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
        }

        std::cout << "dynamic alignment = " << dynamicAlignment << "\n";
        std::cout << "MVP size = " << sizeof(MVP) << "\n";
        std::cout << "size should be = " << (sizeof(glm::mat4) * 3) + (sizeof(glm::vec3) * 2) << "\n";

        // nr of meshes times the alignment
        bufferSize = meshes.size() * dynamicAlignment;
        std::cout << "bufferSize = " << bufferSize << '\n';
        uboDynamic.mvp = (MVP*)malloc(bufferSize);
        memset(uboDynamic.mvp, 1, bufferSize);

        createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            uniformBuffer,
            uniformBuffersMemory
        );

        vkMapMemory(context.device, uniformBuffersMemory, 0, VK_WHOLE_SIZE, 0, &mapped);
        memcpy(mapped, uboDynamic.mvp, bufferSize);

        VkMappedMemoryRange memoryRange = {};
        memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        memoryRange.memory = uniformBuffersMemory;
        memoryRange.size = bufferSize;
        vkFlushMappedMemoryRanges(context.device, 1, &memoryRange);

        VkDescriptorSetAllocateInfo desc_info = {};
        desc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        desc_info.descriptorPool = context.device.descriptorPool;
        desc_info.descriptorSetCount = 1;
        desc_info.pSetLayouts = &descriptorSetLayout;

        if (vkAllocateDescriptorSets(context.device, &desc_info, &descriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets");
        }

        std::vector<VkDescriptorImageInfo> images = {};
        for (const VKTexture& texture : textures) {
            VkDescriptorImageInfo info = {};
            info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            info.imageView = texture.getView();
            info.sampler = texture.getSampler();
            images.push_back(info);
        }

        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = uniformBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet buffer_descriptor = {};
        buffer_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        buffer_descriptor.dstSet = descriptorSet;
        buffer_descriptor.dstBinding = 0;
        buffer_descriptor.dstArrayElement = 0;
        buffer_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        buffer_descriptor.descriptorCount = 1;
        buffer_descriptor.pBufferInfo = &bufferInfo;
        buffer_descriptor.pImageInfo = nullptr;
        buffer_descriptor.pTexelBufferView = nullptr;

        VkWriteDescriptorSet image_descriptor = {};
        image_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        image_descriptor.dstSet = descriptorSet;
        image_descriptor.dstBinding = 1;
        image_descriptor.dstArrayElement = 0;
        image_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        image_descriptor.descriptorCount = static_cast<uint32_t>(images.size());
        image_descriptor.pImageInfo = images.data();

        std::array<VkWriteDescriptorSet, 2> descriptors = { buffer_descriptor, image_descriptor };
        vkUpdateDescriptorSets(context.device, static_cast<uint32_t>(descriptors.size()), descriptors.data(), 0, nullptr);
    }

    void setupSkyboxStageUniformBuffers() {
        VkDescriptorSetLayoutBinding uboLayoutBinding = {};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.pImmutableSamplers = nullptr; // Optional
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &descriptorSetLayout2) != VK_SUCCESS) {
            throw std::runtime_error("fialed to create descriptor layout");
        }

        createBuffer(
            sizeof(skyboxmvp),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uniformBuffer2,
            uniformBuffersMemory2
        );

        VkDescriptorSetAllocateInfo desc_info = {};
        desc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        desc_info.descriptorPool = context.device.descriptorPool;
        desc_info.descriptorSetCount = 1;
        desc_info.pSetLayouts = &descriptorSetLayout2;

        if (vkAllocateDescriptorSets(context.device, &desc_info, &descriptorSet2) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets");
        }

        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = uniformBuffer2;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(skyboxmvp);

        VkDescriptorImageInfo info = {};
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        info.imageView = skybox.getView();
        info.sampler = skybox.getSampler();

        VkWriteDescriptorSet buffer_descriptor2 = {};
        buffer_descriptor2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        buffer_descriptor2.dstSet = descriptorSet2;
        buffer_descriptor2.dstBinding = 0;
        buffer_descriptor2.dstArrayElement = 0;
        buffer_descriptor2.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        buffer_descriptor2.descriptorCount = 1;
        buffer_descriptor2.pBufferInfo = &bufferInfo;
        buffer_descriptor2.pImageInfo = nullptr;
        buffer_descriptor2.pTexelBufferView = nullptr;

        VkWriteDescriptorSet image_descriptor2 = {};
        image_descriptor2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        image_descriptor2.dstSet = descriptorSet2;
        image_descriptor2.dstBinding = 1;
        image_descriptor2.dstArrayElement = 0;
        image_descriptor2.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        image_descriptor2.descriptorCount = 1;
        image_descriptor2.pImageInfo = &info;

        std::array<VkWriteDescriptorSet, 2> descriptors2 = { buffer_descriptor2, image_descriptor2 };
        vkUpdateDescriptorSets(context.device, static_cast<uint32_t>(descriptors2.size()), descriptors2.data(), 0, nullptr);
    }

    void setupFrameBuffers() {
        depth_texture = createDepthTexture();
        swapchain.setupFrameBuffers(context, renderPass, { depth_texture.getView() });
    }

    void setupSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        imagesInFlight.resize(swapchain.images.size(), VK_NULL_HANDLE);

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS)
                throw std::runtime_error("failed to create semaphore");
            if (vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
                throw std::runtime_error("failed to create semaphore");
            if (vkCreateFence(context.device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
                throw std::runtime_error("failed to create semaphore");
        }
    }

    void allocateCommandBuffers() {
        // allocate main dynamic buffers
        VkCommandBufferAllocateInfo primaryInfo = {};
        primaryInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        primaryInfo.commandPool = context.device.commandPool;
        primaryInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        primaryInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(context.device, &primaryInfo, &maincmdbuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate vk command buffers");
        }

        // allocate static buffers for meshes
        VkCommandBufferAllocateInfo secondaryInfo = {};
        secondaryInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        secondaryInfo.commandPool = context.device.commandPool;
        secondaryInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        secondaryInfo.commandBufferCount = 1;

        for (int i = 0; i < meshes.size(); i++) {
            secondaryBuffers.push_back(VkCommandBuffer());
            vkAllocateCommandBuffers(context.device, &secondaryInfo, &secondaryBuffers.back());
        }

        // allocate imgui cmd buffer
        if (vkAllocateCommandBuffers(context.device, &secondaryInfo, &imguicmdbuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate vk command buffers");
        }

        // allocate skybox cmd buffer
        if (vkAllocateCommandBuffers(context.device, &secondaryInfo, &skyboxcmdbuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate vk command buffers");
        }
    }

    template<typename T>
    void updateUniformBuffer(const T& ubo) {
        void* data;
        vkMapMemory(context.device, uniformBuffersMemory, 0, sizeof(T), 0, &data);
        memcpy(data, &ubo, sizeof(T));
        vkUnmapMemory(context.device, uniformBuffersMemory);
    }

    template<typename T>
    void updateSkyboxUniformBuffer(const T& ubo) {
        void* data;
        vkMapMemory(context.device, uniformBuffersMemory2, 0, sizeof(T), 0, &data);
        memcpy(data, &ubo, sizeof(T));
        vkUnmapMemory(context.device, uniformBuffersMemory2);
    }

    uint32_t getNextFrame() {
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(context.device, swapchain, UINT64_MAX, imageAvailableSemaphores[current_frame], VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain(vsync);
            return current_frame;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image");
        }
        
        return imageIndex;
    }

    void waitForIdle() {
        if (vkDeviceWaitIdle(context.device) != VK_SUCCESS) {
            throw std::runtime_error("failed to wait for the gpu to idle");
        }
    }

    void recordSkyboxBuffer(VK::VertexBuffer* cubeVertices, VK::IndexBuffer* cubeIndices, VkPipelineLayout& pLayout, VkDescriptorSet& set, VkCommandBuffer& cmdbuffer) {
        VkCommandBufferInheritanceInfo inherit_info = {};
        inherit_info.renderPass = renderPass;
        inherit_info.subpass = 0;
        inherit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;

        // create secondary command buffer begin info
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT; // Optional
        beginInfo.pInheritanceInfo = &inherit_info; // Optional

        if (vkBeginCommandBuffer(cmdbuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin command buffer recording");
        }
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &cubeVertices->buffer, offsets);
        vkCmdBindIndexBuffer(cmdbuffer, cubeIndices->buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);
        vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pLayout, 0, 1, &set, 0, nullptr);

        std::cout << cubeIndices->getCount() << std::endl;
        vkCmdDrawIndexed(cmdbuffer, cubeIndices->getCount(), 1, 0, 0, 0);
        if (vkEndCommandBuffer(cmdbuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to end command buffer");
        }
    }

    void recordMeshBuffer(uint32_t bufferIndex, VKMesh& m, VkPipelineLayout& pipelineLayout, VkDescriptorSet& descriptorSets, VkCommandBuffer& cmdbuffer) {
        // record static mesh command buffer
        // allocate static buffers for meshes
        VkCommandBufferInheritanceInfo inherit_info = {};
        inherit_info.renderPass = renderPass;
        inherit_info.subpass = 0;
        inherit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;

        // create secondary command buffer begin info
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT; // Optional
        beginInfo.pInheritanceInfo = &inherit_info; // Optional

        if (vkBeginCommandBuffer(cmdbuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin command buffer recording");
        }
        VkDeviceSize offsets[] = { 0 };
        vkCmdPushConstants(cmdbuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &m.textureIndex);
        vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &meshVertexBuffer->buffer, offsets);
        vkCmdBindIndexBuffer(cmdbuffer, meshIndexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        uint32_t offset = static_cast<uint32_t>(bufferIndex * dynamicAlignment);
        vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout, 0, 1, &descriptorSets, 1, &offset);

        vkCmdDrawIndexed(cmdbuffer, m.indexRange, 1, m.indexOffset, m.vertexOffset, 0);
        if (vkEndCommandBuffer(cmdbuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to end command buffer");
        }
    }

    void createGraphicsPipeline(std::array<VkPipelineShaderStageCreateInfo, 2> shaders, std::array<VkPipelineShaderStageCreateInfo, 2> skyboxShaders) {
        // describe the topology used, like in directx
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // viewport and scissoring, fairly straight forward
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = (float)swapchain.extent.height;
        viewport.width = (float)swapchain.extent.width;
        viewport.height = (-(float)swapchain.extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = swapchain.extent;

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        // setup the vk rasterizer, also straight forward
        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional

        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        VkPushConstantRange pcr = {};
        pcr.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
        pcr.offset = 0;
        pcr.size = sizeof(uint32_t);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pcr;

        if (vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout");
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo2 = {};
        pipelineLayoutInfo2.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo2.setLayoutCount = 1;
        pipelineLayoutInfo2.pSetLayouts = &descriptorSetLayout2;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pcr;

        if (vkCreatePipelineLayout(context.device, &pipelineLayoutInfo2, nullptr, &pipelineLayout2) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout");
        }

        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = swapchain.imageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depthAttachment = {};
        depthAttachment.format = findSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
            VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef = {};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference ref = {};

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(context.device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vk render pass");
        }

        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f; // Optional
        depthStencil.maxDepthBounds = 1.0f; // Optional
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {}; // Optional
        depthStencil.back = {}; // Optional

        {
            VkGraphicsPipelineCreateInfo pipelineInfo = {};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = shaders.data();
            pipelineInfo.pVertexInputState = &input_state;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &rasterizer;
            pipelineInfo.pMultisampleState = &multisampling;
            pipelineInfo.pDepthStencilState = nullptr; // Optional
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.pDynamicState = nullptr; // Optional
            pipelineInfo.layout = pipelineLayout;
            pipelineInfo.renderPass = renderPass;
            pipelineInfo.subpass = 0;
            pipelineInfo.pDepthStencilState = &depthStencil;

            if (vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
                throw std::runtime_error("failed to create vk final graphics pipeline");
            }
        }

        {
            depthStencil.depthTestEnable = VK_FALSE;
            depthStencil.depthWriteEnable = VK_FALSE;
            rasterizer.cullMode = VK_CULL_MODE_NONE;

            VkGraphicsPipelineCreateInfo pipelineInfo = {};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = skyboxShaders.data();
            pipelineInfo.pVertexInputState = &input_state;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &rasterizer;
            pipelineInfo.pMultisampleState = &multisampling;
            pipelineInfo.pDepthStencilState = nullptr; // Optional
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.pDynamicState = nullptr; // Optional
            pipelineInfo.layout = pipelineLayout2;
            pipelineInfo.renderPass = renderPass;
            pipelineInfo.subpass = 0;
            pipelineInfo.pDepthStencilState = &depthStencil;

            if (vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &skyboxPipeline) != VK_SUCCESS) {
                throw std::runtime_error("failed to create vk final graphics pipeline");
            }
        }
    }

    void ImGuiRecord() {
        VkCommandBufferInheritanceInfo inherit_info = {};
        inherit_info.renderPass = renderPass;
        inherit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;

        // create secondary command buffer begin info
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT; // Optional
        beginInfo.pInheritanceInfo = &inherit_info; // Optional

        if (vkBeginCommandBuffer(imguicmdbuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin command buffer recording");
        }
        
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), imguicmdbuffer);

        if (vkEndCommandBuffer(imguicmdbuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to end command buffer");
        }
    }

    void render(uint32_t imageIndex, const glm::mat4& sky_transform) {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        VkRenderPassBeginInfo render_info = {};
        render_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_info.renderPass = renderPass;
        render_info.framebuffer = swapchain.framebuffers[imageIndex];
        render_info.renderArea.offset = { 0, 0 };
        render_info.renderArea.extent = swapchain.extent;

        std::array<VkClearValue, 2> clearValues = {};
        clearValues[0].color = { 1.0f, 1.0f, 0.0f, 1.0f };
        clearValues[1].depthStencil = { 1.0f, 0 };

        render_info.clearValueCount = static_cast<uint32_t>(clearValues.size());
        render_info.pClearValues = clearValues.data();

        if (vkBeginCommandBuffer(maincmdbuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        }

        // start the render pass and execute secondary command buffers
        vkCmdBeginRenderPass(maincmdbuffer, &render_info, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

        // update the mvp without translation and execute the skybox render commands
        uniformBufferObject.MVP = sky_transform;
        updateSkyboxUniformBuffer(uniformBufferObject);
        vkCmdExecuteCommands(maincmdbuffer, 1, &skyboxcmdbuffer);

        memcpy(mapped, uboDynamic.mvp, bufferSize);

        VkMappedMemoryRange memoryRange = {};
        memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        memoryRange.memory = uniformBuffersMemory;
        memoryRange.size = bufferSize;
        vkFlushMappedMemoryRanges(context.device, 1, &memoryRange);

        if (secondaryBuffers.data() != nullptr) {
            vkCmdExecuteCommands(maincmdbuffer, static_cast<uint32_t>(secondaryBuffers.size()), secondaryBuffers.data());
        }

        vkCmdExecuteCommands(maincmdbuffer, 1, &imguicmdbuffer);
        vkCmdEndRenderPass(maincmdbuffer);

        if (vkEndCommandBuffer(maincmdbuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to end command buffer");
        }

        // Check if a previous frame is using this image (i.e. there is its fence to wait on)
        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(context.device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        // Mark the image as now being in use by this frame
        imagesInFlight[imageIndex] = inFlightFences[current_frame];


        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[current_frame] };
        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[current_frame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        std::array<VkCommandBuffer, 1> cmdbuffers = { maincmdbuffer };

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = static_cast<uint32_t>(cmdbuffers.size());
        submitInfo.pCommandBuffers = cmdbuffers.data();
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        // reset command buffer fences 
        vkResetFences(context.device, 1, &inFlightFences[current_frame]);

        if (vkQueueSubmit(context.device.graphicsQueue, 1, &submitInfo, inFlightFences[current_frame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit queue");
        }

        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { swapchain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr; // Optional

        auto result = vkQueuePresentKHR(context.device.presentQueue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            recreateSwapchain(vsync);
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        vkWaitForFences(context.device, 1, &inFlightFences[current_frame], VK_TRUE, UINT64_MAX);
        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(context.PDevice, format, &props);
            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            }
            else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }
        throw std::runtime_error("unable to find a supported format");
        return {};
    };

    VKTexture createDepthTexture() {
        VkImage depth_image;
        VkDeviceMemory depth_mem;
        VkImageView depth_view;

        VkFormat depthFormat = findSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
            VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

        createImage(swapchain.extent.width, swapchain.extent.height,
            1, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            depth_image, depth_mem);

        depth_view = context.device.createImageView(depth_image, depthFormat, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1);

        context.device.transitionImageLayout(depth_image, depthFormat, 1, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        return VKTexture(depth_image, depth_mem, depth_view);
    }

    VKTexture loadTexture(const stbData& stb) {
        uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(stb.w, stb.h)))) + 1;

        VkDeviceSize image_size = stb.w * stb.h * 4;
        VkBuffer stage_pixels;
        VkDeviceMemory stage_pixels_mem;

        createBuffer(
            image_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stage_pixels,
            stage_pixels_mem
        );

        void* pdata;
        vkMapMemory(context.device, stage_pixels_mem, 0, image_size, 0, &pdata);
        memcpy(pdata, stb.pixels, static_cast<size_t>(image_size));
        vkUnmapMemory(context.device, stage_pixels_mem);

        stbi_image_free(stb.pixels);

        VkImage texture;
        VkDeviceMemory texture_mem;

        createImage(
            stb.w, stb.h, mipLevels, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture, texture_mem
        );

        context.device.transitionImageLayout(texture, VK_FORMAT_R8G8B8A8_UNORM, mipLevels, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        context.device.copyBufferToImage(stage_pixels, texture, static_cast<uint32_t>(stb.w), static_cast<uint32_t>(stb.h));
        context.device.generateMipmaps(texture, stb.w, stb.h, mipLevels);
        //transitionImageLayout(texture, VK_FORMAT_R8G8B8A8_UNORM, mipLevels, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vkDestroyBuffer(context.device, stage_pixels, nullptr);
        vkFreeMemory(context.device, stage_pixels_mem, nullptr);

        auto texture_image_view = context.device.createImageView(texture, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, 1);

        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = static_cast<float>(mipLevels);

        VkSampler sampler;
        if (vkCreateSampler(context.device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vk sampler");
        }

        return VKTexture(texture, texture_mem, texture_image_view, sampler);
    }

    VKTexture loadTexture(const std::string& path) {
        stbi_set_flip_vertically_on_load(true);
        stbData stb = {};
        // we shouldnt have to flip texture for vulkan, TODO: figure out why everything is upside down
        stb.pixels = stbi_load(path.c_str(), &stb.w, &stb.h, &stb.ch, STBI_rgb_alpha);
        if (!stb.pixels) {
            std::cout << "failed to load \"" << path << "\" \n";
            throw std::runtime_error("failed to laod stb image for texturing");
        }
        else {
            std::cout << "loaded " << path << '\n';
        }
        return loadTexture(stb);
    }

    VKTexture load_skybox() {
        stbi_set_flip_vertically_on_load(false);
        unsigned char* textureData[6];
        int width, height, n_channels;
        for (unsigned int i = 0; i < 6; i++) {
            textureData[i] = stbi_load(face_files[i].c_str(), &width, &height, &n_channels, 4);
            m_assert(textureData[i], "failed to load image");
        }
        //Calculate the image size and the layer size.
        const VkDeviceSize imageSize = width * height * 4 * 6;
        const VkDeviceSize layerSize = imageSize / 6;

        // setup the stage buffer
        VkBuffer stage_pixels;
        VkDeviceMemory stage_pixels_mem;

        createBuffer(
            imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stage_pixels,
            stage_pixels_mem
        );

        // map and copy the memory over
        void* pdata;
        vkMapMemory(context.device, stage_pixels_mem, 0, imageSize, 0, &pdata);
        for (unsigned int i = 0; i < 6; i++) {
            unsigned char* location = static_cast<unsigned char*>(pdata) + (layerSize * i);
            memcpy(location, textureData[i], layerSize);
        }
        vkUnmapMemory(context.device, stage_pixels_mem);

        // cleanup the loaded image data from RAM
        for (unsigned int i = 0; i < 6; i++) {
            stbi_image_free(textureData[i]);
        }

        VkImage texture;
        VkDeviceMemory texture_mem;

        // create the image
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 6;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        if (vkCreateImage(context.device, &imageInfo, nullptr, &texture) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vk image");
        }

        // allocate and bind GPU memory for us to use
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(context.device, texture, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(context.device, &allocInfo, nullptr, &texture_mem) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image meory");
        }

        vkBindImageMemory(context.device, texture, texture_mem, 0);

        constexpr uint32_t mipLevels = 1;
        constexpr uint32_t layerCount = 6;
        context.device.transitionImageLayout(texture, VK_FORMAT_R8G8B8A8_UNORM, mipLevels, layerCount, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        context.device.copyBufferToImage(stage_pixels, texture, static_cast<uint32_t>(width), static_cast<uint32_t>(height), 6);
        context.device.transitionImageLayout(texture, VK_FORMAT_R8G8B8A8_UNORM, mipLevels, layerCount, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vkDestroyBuffer(context.device, stage_pixels, nullptr);
        vkFreeMemory(context.device, stage_pixels_mem, nullptr);

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = texture;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 6;

        VkImageView imageView;
        if (vkCreateImageView(context.device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }

        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 1.0f;

        VkSampler sampler;
        if (vkCreateSampler(context.device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vk sampler");
        }

        return VKTexture(texture, texture_mem, imageView, sampler);
    }

    void createImage(uint32_t w, uint32_t h, uint32_t mipLevels, uint32_t layers, VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = w;
        imageInfo.extent.height = h;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(context.device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vk image");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(context.device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(context.device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image meory");
        }

        vkBindImageMemory(context.device, image, imageMemory, 0);
    };

    void recreateSwapchain(bool useVsync) {
        vsync = useVsync;
        try {
            waitForIdle();
            int w, h;
            SDL_GetWindowSize(context.window, &w, &h);
            uint32_t flags = SDL_GetWindowFlags(context.window);
            while (flags & SDL_WINDOW_MINIMIZED) {
                flags = SDL_GetWindowFlags(context.window);
                SDL_Event ev;
                while (SDL_PollEvent(&ev)) {}
            }
            // recreate the swapchain
            cleanupSwapChain();
            VkPresentModeKHR mode = useVsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
            swapchain.recreate(context, { w, h }, mode);

            std::array<VkPipelineShaderStageCreateInfo, 2> shaders = {
                vert.getInfo(VK_SHADER_STAGE_VERTEX_BIT),
                frag.getInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
            };

            std::array<VkPipelineShaderStageCreateInfo, 2> shaders2 = {
                skyboxv.getInfo(VK_SHADER_STAGE_VERTEX_BIT),
                skyboxf.getInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
            };

            // create the graphics pipeline
            createGraphicsPipeline(shaders, shaders2);
            setupFrameBuffers();
            recordModel();
            recordSkyboxBuffer(cube_v.get(), cube_i.get(), pipelineLayout2, descriptorSet2, skyboxcmdbuffer);
        }
        catch (std::exception e) {
            std::cout << e.what() << '\n';
            abort();
        }
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(context.PDevice, &memProperties);
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        return UINT32_MAX;
    };

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(context.device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(context.device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(context.device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        vkBindBufferMemory(context.device, buffer, bufferMemory, 0);
    };

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = context.device.beginSingleTimeCommands();

        VkBufferCopy copyRegion = {};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        context.device.endSingleTimeCommands(commandBuffer);
    };

    void ImGuiInit(SDL_Window* window) {
        ImGui_ImplVulkan_InitInfo info = {};
        info.Device = context.device;
        info.PhysicalDevice = context.PDevice;
        info.Instance = context.instance;
        info.QueueFamily = context.device.getQueues().graphics.value();
        info.Queue = context.device.graphicsQueue;
        info.PipelineCache = VK_NULL_HANDLE;
        info.DescriptorPool = context.device.descriptorPool;
        info.MinImageCount = 2;
        info.ImageCount = info.MinImageCount;
        info.Allocator = nullptr;
        info.CheckVkResultFn = nullptr;
        ImGui_ImplSDL2_InitForVulkan(window);
        ImGui_ImplVulkan_Init(&info, renderPass);
    }

    void ImGuiCreateFonts() {
        VkCommandBuffer command_buffer = context.device.beginSingleTimeCommands();
        ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
        context.device.endSingleTimeCommands(command_buffer);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    void ImGuiNewFrame(SDL_Window* window) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();
    }
};

bool Application::running = true;
bool Application::showUI = true;
bool Application::shouldResize = false;

void Application::vulkan_main() {
    auto context = Raekor::PlatformContext();

    // retrieve the application settings from the config file
    serialize_settings("config.json");

    int sdl_err = SDL_Init(SDL_INIT_VIDEO);
    m_assert(sdl_err == 0, "failed to init SDL for video");

    Uint32 wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN |
        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN;

    std::vector<SDL_Rect> displays;
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        displays.push_back(SDL_Rect());
        SDL_GetDisplayBounds(i, &displays.back());
    }

    // if our display setting is higher than the nr of displays we pick the default display
    auto index = display > displays.size() - 1 ? 0 : display;
    auto window = SDL_CreateWindow(name.c_str(),
        SDL_WINDOWPOS_CENTERED_DISPLAY(index),
        SDL_WINDOWPOS_CENTERED_DISPLAY(index),
        static_cast<int>(displays[index].w * 0.9),
        static_cast<int>(displays[index].h * 0.9),
        wflags);

     //initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigDockingWithShift = true;
    ImFont* pFont = io.Fonts->AddFontFromFileTTF(font.c_str(), 18.0f);
    if (!io.Fonts->Fonts.empty()) {
        io.FontDefault = io.Fonts->Fonts.back();
    }

    Camera camera = Camera({ 0.0f, 1.0f, 0.0f }, 45.0f);

    face_files = skyboxes["lake"];
    VKRender vk = VKRender(window);

    vk.ImGuiInit(window);
    vk.ImGuiCreateFonts();

    std::puts("job well done.");

    SDL_ShowWindow(window);
    SDL_SetWindowInputFocus(window);

    Ffilter ft_mesh = {};
    ft_mesh.name = "Supported Mesh Files";
    ft_mesh.extensions = "*.obj;*.fbx";

    // MVP uniform buffer object
    skyboxmvp ubo = {};
    int active = 0;

    std::vector<mod> mods = std::vector<mod>(vk.getMeshCount());
    for (mod& m : mods) {
        m.model = glm::mat4(1.0f);
        m.transform();
    }

    Timer dt_timer = Timer();
    double dt = 0;

    glm::mat4 lightmatrix = glm::mat4(1.0f);
    lightmatrix = glm::translate(lightmatrix, { 0.0, 1.0f, 0.0 });
    float lightPos[3], lightRot[3], lightScale[3];
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(lightmatrix), lightPos, lightRot, lightScale);
    glm::vec4 lightAngle = { 0.0f, 1.0f, 1.0f, 0.0f };

    bool use_vsync = true;
    bool update = false;

    //main application loop
    while (running) {
        dt_timer.start();
        //handle sdl and imgui events
        handle_sdl_gui_events({ window }, camera, dt);

        // update the mvp structs
        for (uint32_t i = 0; i < mods.size(); i++) {
            MVP* modelMat = (MVP*)(((uint64_t)uboDynamic.mvp + (i * dynamicAlignment)));
            modelMat->model = mods[i].model;
            modelMat->projection = camera.getProjection();
            modelMat->view = camera.getView();
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(lightmatrix), lightPos, lightRot, lightScale);
            modelMat->lightPos = glm::vec4(glm::make_vec3(lightPos), 1.0f);
            modelMat->lightAngle = lightAngle;
        }

        uint32_t frame = vk.getNextFrame();
        vk.updateSkyboxUniformBuffer(ubo);

        // start a new imgui frame
        vk.ImGuiNewFrame(window);
        ImGuizmo::BeginFrame();
        ImGuizmo::Enable(true);

        static bool opt_fullscreen_persistant = true;
        bool opt_fullscreen = opt_fullscreen_persistant;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_DockNodeHost;
        if (opt_fullscreen) {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
        }

        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the pass-thru hole, 
        // so we ask Begin() to not render a background.
        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        static bool p_open = true;
        ImGui::Begin("DockSpace", &p_open, window_flags);
        ImGui::PopStyleVar();
        if (opt_fullscreen) ImGui::PopStyleVar(2);

        // DockSpace
        ImGuiIO& io = ImGui::GetIO();
        ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(io.DisplaySize.x, io.DisplaySize.y), dockspace_flags);
        }

        // move the light by a fixed amount and let it bounce between -125 and 125 units/pixels on the x axis
        static double move_amount = 0.003;
        static double bounds = 10.0f;
        static bool move_light = true;
        double light_delta = move_amount * dt;
        if ((lightPos[0] >= bounds && move_amount > 0) || (lightPos[0] <= -bounds && move_amount < 0)) {
            move_amount *= -1;
        }
        if (move_light) {
            lightmatrix = glm::translate(lightmatrix, { light_delta, 0.0, 0.0 });
        }

        if (showUI) {
            // draw the imguizmo at the center of the light
            ImGuizmo::Manipulate(glm::value_ptr(camera.getView()), glm::value_ptr(camera.getProjection()), ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::MODE::WORLD, glm::value_ptr(lightmatrix));
            ImGui::Begin("ECS", (bool*)0, ImGuiWindowFlags_AlwaysAutoResize);
            if (ImGui::Button("Add Model")) {
                std::string path = context.open_file_dialog({ ft_mesh });
                if (!path.empty()) {
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove Model")) {
            }
            ImGui::End();

            ImGui::ShowMetricsWindow();

            ImGui::Begin("Mesh Properties");

            if (ImGui::SliderInt("Mesh", &active, 0, 24)) {}
            if (ImGui::DragFloat3("Scale", glm::value_ptr(mods[active].scale), 0.01f, 0.0f, 10.0f)) {
                mods[active].transform();
            }
            if (ImGui::DragFloat3("Position", glm::value_ptr(mods[active].position), 0.01f, -100.0f, 100.0f)) {
                mods[active].transform();
            }
            if (ImGui::DragFloat3("Rotation", glm::value_ptr(mods[active].rotation), 0.01f, (float)(-M_PI), (float)(M_PI))) {
                mods[active].transform();
            }
            ImGui::End();

            ImGui::Begin("Camera Properties");
            if (ImGui::DragFloat("Camera Move Speed", camera.get_move_speed(), 0.001f, 0.01f, FLT_MAX, "%.2f")) {}
            if (ImGui::DragFloat("Camera Look Speed", camera.get_look_speed(), 0.0001f, 0.0001f, FLT_MAX, "%.4f")) {}

            ImGui::End();

            // scene panel
            ImGui::Begin("Scene");
            // toggle button for vsync
            if (ImGui::RadioButton("USE VSYNC", use_vsync)) {
                use_vsync = !use_vsync;
                update = true;
            } 
            if (ImGui::RadioButton("Animate Light", move_light)) {
                move_light = !move_light;
            }

            if (ImGui::Button("Reload shaders")) {
                vk.reloadShaders();
            }

            ImGui::NewLine(); ImGui::Separator();

            ImGui::Text("Light Properties");
            if (ImGui::DragFloat3("Angle", glm::value_ptr(lightAngle), 0.01f, -1.0f, 1.0f)) {}


            ImGui::End();
        }


        // End DOCKSPACE
        ImGui::End();

        // tell imgui to collect render data
        ImGui::Render();
        // record the collected data to secondary command buffers
        vk.ImGuiRecord();
        // start the overall render pass
        camera.update();
        
        glm::mat4 sky_matrix = camera.getProjection() * glm::mat4(glm::mat3(camera.getView())) * glm::mat4(1.0f);

        vk.render(frame, sky_matrix);
        // tell imgui we're done with the current frame
        ImGui::EndFrame();

        if (update || shouldResize) {
            vk.recreateSwapchain(use_vsync);
            update = false;
            shouldResize = false;
        }

        dt_timer.stop();
        dt = dt_timer.elapsed_ms();
    }

    vk.waitForIdle();

    SDL_DestroyWindow(window);
    SDL_Quit();
}

} // namespace Raekor