#include "pch.h"
#include "app.h"
#include "mesh.h"
#include "util.h"
#include "timer.h"
#include "entry.h"
#include "camera.h"
#include "buffer.h"
#include "PlatformContext.h"

#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/Importer.hpp"

#define _glslc "dependencies\\glslc.exe "
#define _cl(in, out) system(std::string(_glslc + static_cast<std::string>(in) + static_cast<std::string>(" -o ") + static_cast<std::string>(out)).c_str())
void compile_shader(const char* in, const char* out) {
    if(_cl(in, out) != 0) {
        std::cout << "failed to compile vulkan shader: " + std::string(in) << '\n';
    } else {
        std::cout << "Successfully compiled VK shader: " + std::string(in) << '\n';

    }
}

namespace Raekor {

struct queue_indices {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;

    bool isComplete() {
        return graphics.has_value() and present.has_value();
    }
};

class VKTexture {
public:
    VKTexture(VkImage& p_image, VkDeviceMemory& p_memory, VkImageView& p_view, VkSampler& p_sampler) :
        image(p_image), memory(p_memory), view(p_view), sampler(p_sampler) {}

    VKTexture(VkImage& p_image, VkDeviceMemory& p_memory, VkImageView& p_view) :
        image(p_image), memory(p_memory), view(p_view), sampler(VK_NULL_HANDLE) {}

    ~VKTexture() {
        
    }

    inline VkImage& getImage() { return image; }
    inline VkSampler& getSampler() { return sampler; }
    inline VkImageView& getView() { return view; }
    inline VkDeviceMemory& getMemory() { return memory; }

private:
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkSampler sampler;
};

class VKBuffer {
public:
    VKBuffer(VkBuffer& p_buffer, VkDeviceMemory& p_memory) 
    : buffer(p_buffer), memory(p_memory) {}

    VkBuffer& getBuffer() {
        return buffer;
    }

    VkDeviceMemory& getMemory() {
        return memory;
    }
private:
    VkBuffer buffer;
    VkDeviceMemory memory;
};

VkFormat vk_format(ShaderType type) {
    switch (type) {
    case ShaderType::FLOAT1: return VK_FORMAT_R32_SFLOAT;
    case ShaderType::FLOAT2: return VK_FORMAT_R32G32_SFLOAT;
    case ShaderType::FLOAT3: return VK_FORMAT_R32G32B32_SFLOAT;
    case ShaderType::FLOAT4: return VK_FORMAT_R32G32B32A32_SFLOAT;
    default: return VK_FORMAT_END_RANGE;
    }
}

class VKIndexBuffer : public VKBuffer {
public:
    VKIndexBuffer(VkBuffer& p_buffer, VkDeviceMemory& p_memory, uint32_t p_count)
        : VKBuffer(p_buffer, p_memory), count(p_count) {}

    uint32_t getCount() {
        return count;
    }
private:
    uint32_t count;
};

class VKVertexBuffer: public VKBuffer {
public:
    VKVertexBuffer(VkBuffer& p_buffer, VkDeviceMemory& p_memory) :
        VKBuffer(p_buffer, p_memory), info({}), bindingDescription({}), layout({}) {
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        info.vertexBindingDescriptionCount = 1;
        info.pVertexBindingDescriptions = &bindingDescription;
    }

    void setLayout(const InputLayout& new_layout) {
        layout.clear();
        uint32_t location = 0;
        for (auto& element : new_layout) {
            VkVertexInputAttributeDescription attrib = {};
            attrib.binding = 0;
            attrib.location = location++;
            attrib.format = vk_format(element.type);
            attrib.offset = element.offset;
            layout.push_back(attrib);
        }
        info.vertexAttributeDescriptionCount = static_cast<uint32_t>(layout.size());
        info.pVertexAttributeDescriptions = layout.data();
    }

    inline VkPipelineVertexInputStateCreateInfo& getVertexInputState() { return info; }

private:
    std::vector<VkVertexInputAttributeDescription> layout;
    VkPipelineVertexInputStateCreateInfo info;
    VkVertexInputBindingDescription bindingDescription;
};

class VKShader {
public:    
    VKShader(const std::string& path) 
        : filepath(path), module(VK_NULL_HANDLE) {}

private:
    std::vector<char> readShaderFile(const std::string& path) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        m_assert(file.is_open(), "failed to open " + path);
        size_t filesize = (size_t)file.tellg();
        std::vector<char> buffer(filesize);
        file.seekg(0);
        file.read(buffer.data(), filesize);
        file.close();
        return buffer;
    }

public:
    void createModule(const VkDevice& device) {
        auto buffer = readShaderFile(filepath);
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = buffer.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());
        if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vk shader module");
        }
    }

    VkPipelineShaderStageCreateInfo getInfo(VkShaderStageFlagBits stage) {
        VkPipelineShaderStageCreateInfo stage_info = {};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = stage;
        stage_info.module = module;
        stage_info.pName = "main";
        return stage_info;
    }

    inline const VkShaderModule& getModule() const {
        return module;
    }

    std::string filepath;
    VkShaderModule module;
};


class VKRender {
private:
    bool enableValidationLayers;

    queue_indices qindices;
	VkInstance instance;
	VkSurfaceKHR surface;
	VkPhysicalDevice gpu;
    VkExtent2D extent;
    VkCommandPool commandPool;
    VkQueue present_q;
    VkQueue graphics_q;
    VkRenderPass renderPass;
	VkDevice device;
    VkDescriptorPool descriptorPool;
    VkDescriptorPool g_DescriptorPool;

    VkPipeline graphicsPipeline;

    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    std::vector<VkImageView> swapChainImageViews;
    VkSwapchainKHR swapchain;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;

    VkCommandBuffer maincmdbuffer;
    VkCommandBuffer meshcmdbuffer;
    VkCommandBuffer imguicmdbuffer;
    std::vector<VkCommandBuffer> secondaryBuffers;

    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBuffersMemory;

    uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    int current_frame = 0;

public:
    ~VKRender() {
        vkDestroyRenderPass(device, renderPass, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        for (auto& imageview : swapChainImageViews)
            vkDestroyImageView(device, imageview, nullptr);
        for (auto& fb : swapChainFramebuffers)
            vkDestroyFramebuffer(device, fb, nullptr);
        for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }
            vkDestroyBuffer(device, uniformBuffer, nullptr);
            vkFreeMemory(device, uniformBuffersMemory, nullptr);
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        vkDestroyDevice(device, nullptr);
    }

	VKRender() {
#ifdef NDEBUG
        enableValidationLayers = false;
#else
        enableValidationLayers = true;
#endif
    }

	void init(SDL_Window* window) {
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo instance_info = {};
        instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_info.pApplicationInfo = &appInfo;

        unsigned int count;
        if (!SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr)) {
            throw std::runtime_error("failed to get vulkan instance extensions");
        }

        std::vector<const char*> extensions = {
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME // Sample additional extension
        };
        size_t additional_extension_count = extensions.size();
        extensions.resize(additional_extension_count + count);

        auto sdl_bool = SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data() + additional_extension_count);
        if (!sdl_bool) {
            throw std::runtime_error("failed to get instance extensions");
        }

        const std::vector<const char*> validationLayers = {
            "VK_LAYER_LUNARG_standard_validation"
        };

        if (enableValidationLayers) {
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

            for (const char* layerName : validationLayers) {
                bool found = false;
                for (const auto& layerProperties : availableLayers) {
                    if (strcmp(layerName, layerProperties.layerName) == 0) {
                        found = true;
                        break;
                    }
                }
                if(!found) throw std::runtime_error("requested validation layer not supported");
            }
        }
        // Now we can make the Vulkan instance
        instance_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        instance_info.ppEnabledExtensionNames = extensions.data();

        if (enableValidationLayers) {
            instance_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            instance_info.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            instance_info.enabledLayerCount = 0;
        }

        if (vkCreateInstance(&instance_info, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vulkan instance");
        }

        if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
            throw std::runtime_error("failed to create surface");
        }

        gpu = getGPU();

        const std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        struct queue_indices {
            std::optional<uint32_t> graphics;
            std::optional<uint32_t> present;

            bool isComplete() {
                return graphics.has_value() && present.has_value();
            }
        };

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueFamilies.data());

        int queue_index = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                qindices.graphics = queue_index;
            }
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(gpu, queue_index, surface, &presentSupport);
            if (queueFamily.queueCount > 0 && presentSupport) {
                qindices.present = queue_index;
            }
            queue_index++;
        }
        if (!qindices.isComplete() || !requiredExtensions.empty()) {
            throw std::runtime_error("queue family and/or extensions failed");
        }

        //
        // VULKAN LOGICAL DEVICE AND PRESENTATION QUEUE STAGE
        //

        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = qindices.graphics.value();
        queueCreateInfo.queueCount = 1;

        float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceFeatures deviceFeatures = {};
        deviceFeatures.samplerAnisotropy = VK_TRUE;

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = { qindices.graphics.value(), qindices.present.value() };

        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkDeviceCreateInfo device_info = {};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        device_info.pQueueCreateInfos = queueCreateInfos.data();
        device_info.pEnabledFeatures = &deviceFeatures;
        device_info.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        device_info.ppEnabledExtensionNames = deviceExtensions.data();


        if (enableValidationLayers) {
            device_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            device_info.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            device_info.enabledLayerCount = 0;
        }

        if (vkCreateDevice(gpu, &device_info, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vk logical device");
        }

        vkGetDeviceQueue(device, qindices.graphics.value(), 0, &graphics_q);
        vkGetDeviceQueue(device, qindices.present.value(), 0, &present_q);

        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = qindices.graphics.value();
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vk command pool");
        }

        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        setupSwapchain(w, h, extensions);

        VKShader vert = VKShader("shaders/vert.spv");
        VKShader frag = VKShader("shaders/frag.spv");
        vert.createModule(device);
        frag.createModule(device);

        VkPipelineShaderStageCreateInfo shaderStages[] = { 
            vert.getInfo(VK_SHADER_STAGE_VERTEX_BIT), 
            frag.getInfo(VK_SHADER_STAGE_FRAGMENT_BIT) 
        };

        std::vector<Vertex> vertices;
        std::vector<Index> indices;

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
        const auto scene = importer.ReadFile("resources/models/chalet.obj", flags);
        m_assert(scene && scene->HasMeshes(), "failed to load mesh");

        auto ai_mesh = scene->mMeshes[0];

        // extract vertices
        vertices.reserve(ai_mesh->mNumVertices);
        for (size_t i = 0; i < vertices.capacity(); i++) {
            Vertex v;
            v.pos = { ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z };
            if (ai_mesh->HasTextureCoords(0)) {
                v.uv = { ai_mesh->mTextureCoords[0][i].x, ai_mesh->mTextureCoords[0][i].y };
            }
            if (ai_mesh->HasNormals()) {
                v.normal = { ai_mesh->mNormals->x, ai_mesh->mNormals->y, ai_mesh->mNormals->z };
            }
            vertices.push_back(std::move(v));
        }
        // extract indices
        indices.reserve(ai_mesh->mNumFaces);
        for (size_t i = 0; i < indices.capacity(); i++) {
            m_assert((ai_mesh->mFaces[i].mNumIndices == 3), "faces require 3 indices");
            indices.push_back({ ai_mesh->mFaces[i].mIndices[0], ai_mesh->mFaces[i].mIndices[1], ai_mesh->mFaces[i].mIndices[2] });
        }

        VKVertexBuffer vertex_buffer = createVertexBuffer(vertices);
        vertex_buffer.setLayout({
            { "POSITION", ShaderType::FLOAT3 },
            { "UV",       ShaderType::FLOAT2 },
            { "NORMAL",   ShaderType::FLOAT3 }
            });

        VKIndexBuffer index_buffer = createIndexBuffer(indices);

        VKTexture test_texture = loadTexture("resources/textures/chalet.jpg");
        VKTexture depth_texture = createDepthTexture();

        //
    //  UNIFORM BUFFERS
    //
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

        VkDescriptorSetLayout descriptorSetLayout;
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("fialed to create descriptor layout");
        }

            createBuffer(
                sizeof(cb_vs),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                uniformBuffer,
                uniformBuffersMemory
            );

        VkDescriptorPoolSize ubo_pool_size = {};
        ubo_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo_pool_size.descriptorCount = static_cast<uint32_t>(swapChainImages.size());

        VkDescriptorPoolSize sampler_pool_size = {};
        sampler_pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sampler_pool_size.descriptorCount = static_cast<uint32_t>(swapChainImages.size());

        std::array<VkDescriptorPoolSize, 2> pool_sizes = { ubo_pool_size, sampler_pool_size };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        pool_info.pPoolSizes = pool_sizes.data();
        pool_info.maxSets = static_cast<uint32_t>(swapChainImages.size());

        if (vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool for UBO");
        }

        std::vector<VkDescriptorSetLayout> layouts(swapChainImages.size(), descriptorSetLayout);
        VkDescriptorSetAllocateInfo desc_info = {};
        desc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        desc_info.descriptorPool = descriptorPool;
        desc_info.descriptorSetCount = 1;
        desc_info.pSetLayouts = layouts.data();

        VkDescriptorSet descriptorSet;
        if (vkAllocateDescriptorSets(device, &desc_info, &descriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets");
        }

        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkDescriptorBufferInfo bufferInfo = {};
            bufferInfo.buffer = uniformBuffer;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(cb_vs);

            VkDescriptorImageInfo image_info = {};
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info.imageView = test_texture.getView();
            image_info.sampler = test_texture.getSampler();

            VkWriteDescriptorSet buffer_descriptor = {};
            buffer_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            buffer_descriptor.dstSet = descriptorSet;
            buffer_descriptor.dstBinding = 0;
            buffer_descriptor.dstArrayElement = 0;
            buffer_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
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
            image_descriptor.descriptorCount = 1;
            image_descriptor.pImageInfo = &image_info;

            std::array<VkWriteDescriptorSet, 2> descriptors = { buffer_descriptor, image_descriptor };
            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptors.size()), descriptors.data(), 0, nullptr);
        }

        // describe the topology used, like in directx
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // viewport and scissoring, fairly straight forward
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)extent.width;
        viewport.height = (float)extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = extent;

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
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
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

        VkPipelineLayout pipelineLayout;
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout");
        }

        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = swapChainImageFormat;
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

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
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

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;

        pipelineInfo.pVertexInputState = &vertex_buffer.getVertexInputState();
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

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vk final graphics pipeline");
        }

        swapChainFramebuffers.resize(swapChainImageViews.size());
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            std::array<VkImageView, 2> attachments = {
                swapChainImageViews[i],
                depth_texture.getView()
            };

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = extent.width;
            framebufferInfo.height = extent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create vk framebuffer");
            }
        }

        // allocate main dynamic buffers
        VkCommandBufferAllocateInfo primaryInfo = {};
        primaryInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        primaryInfo.commandPool = commandPool;
        primaryInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        primaryInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device, &primaryInfo, &maincmdbuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate vk command buffers");
        }

        // allocate static buffers for meshes
        VkCommandBufferAllocateInfo secondaryInfo = {};
        secondaryInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        secondaryInfo.commandPool = commandPool;
        secondaryInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        secondaryInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device, &secondaryInfo, &meshcmdbuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate vk command buffers");
        }

        if (vkAllocateCommandBuffers(device, &secondaryInfo, &imguicmdbuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate vk command buffers");
        }

        constexpr int MAX_FRAMES_IN_FLIGHT = 2;
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS)
                throw std::runtime_error("failed to create semaphore");
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
                throw std::runtime_error("failed to create semaphore");
            if (vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
                throw std::runtime_error("failed to create semaphore");
        }

        recordMeshBuffer(vertex_buffer, index_buffer, pipelineLayout, descriptorSet, meshcmdbuffer);
	}

    template<typename T>
    void updateUniformBuffer(const T& ubo, uint32_t imageIndex) {
        void* data;
        vkMapMemory(device, uniformBuffersMemory, 0, sizeof(T), 0, &data);
        memcpy(data, &ubo, sizeof(T));
        vkUnmapMemory(device, uniformBuffersMemory);
    }

    uint32_t getNextFrame() {
        uint32_t imageIndex;
        vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[current_frame], VK_NULL_HANDLE, &imageIndex);
        return imageIndex;
    }

    void waitForIdle() {
        vkDeviceWaitIdle(device);
    }

    void recordMeshBuffer(VKVertexBuffer& meshBuffer, VKIndexBuffer& indexBuffer, VkPipelineLayout& pipelineLayout, VkDescriptorSet& descriptorSets, VkCommandBuffer& cmdbuffer) {
        // record static mesh command buffer
            // allocate static buffers for meshes
            VkCommandBufferInheritanceInfo inherit_info = {};
            inherit_info.renderPass = renderPass;
            inherit_info.subpass = 0;
            inherit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;

            auto meshCommands = [&]() {
                std::vector<VkBuffer> vertexBuffers = { meshBuffer.getBuffer() };
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(cmdbuffer, 0, static_cast<uint32_t>(vertexBuffers.size()), vertexBuffers.data(), offsets);
                vkCmdBindIndexBuffer(cmdbuffer, indexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);

                vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
                vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipelineLayout, 0, 1, &descriptorSets, 0, nullptr);

                vkCmdDrawIndexed(cmdbuffer, indexBuffer.getCount(), 1, 0, 0, 0);
            };

            recordSecondaryCommandBuffer(
                cmdbuffer,
                meshCommands,
                &inherit_info,
                VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT
            );
    }

    //for mesh in model:
    //    load vertices, indices
    //    create vkbuffers

    //    b = createcommandbuffers()
    //    func = {bind, draw}
    //    record(b, func)


    void recordSecondaryCommandBuffer(VkCommandBuffer& buffer, std::function<void()> commands, const VkCommandBufferInheritanceInfo* inheritInfo, VkCommandBufferUsageFlags beginFlag) {
        // create secondary command buffer begin info
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = beginFlag; // Optional
        beginInfo.pInheritanceInfo = inheritInfo; // Optional

        if (vkBeginCommandBuffer(buffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin command buffer recording");
        }
        commands();
        if (vkEndCommandBuffer(buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to end command buffer");
        }
    }

    void ImGuiRecord() {
        VkCommandBufferInheritanceInfo inherit_info = {};
        inherit_info.renderPass = renderPass;
        inherit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;

        auto executeCommands = [&]() {
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), imguicmdbuffer);
        };

        recordSecondaryCommandBuffer(
            imguicmdbuffer, 
            executeCommands, 
            &inherit_info, 
            VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT
        );
    }

    void render(uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        VkRenderPassBeginInfo render_info = {};
        render_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_info.renderPass = renderPass;
        render_info.framebuffer = swapChainFramebuffers[imageIndex];
        render_info.renderArea.offset = { 0, 0 };
        render_info.renderArea.extent = extent;

        std::array<VkClearValue, 2> clearValues = {};
        clearValues[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };
        clearValues[1].depthStencil = { 1.0f, 0 };

        render_info.clearValueCount = static_cast<uint32_t>(clearValues.size());
        render_info.pClearValues = clearValues.data();

        if (vkBeginCommandBuffer(maincmdbuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        }

        // start the render pass and execute secondary command buffers
        vkCmdBeginRenderPass(maincmdbuffer, &render_info, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
        vkCmdExecuteCommands(maincmdbuffer, 1, &meshcmdbuffer);
        vkCmdExecuteCommands(maincmdbuffer, 1, &imguicmdbuffer);
        if (secondaryBuffers.data() != nullptr) {
            for (const auto& buffer : secondaryBuffers) {
                vkCmdExecuteCommands(maincmdbuffer, 1, &buffer);
            }
        }
        vkCmdEndRenderPass(maincmdbuffer);

        if (vkEndCommandBuffer(maincmdbuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to end command buffer");
        }

        // Check if a previous frame is using this image (i.e. there is its fence to wait on)
        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        // Mark the image as now being in use by this frame
        imagesInFlight[imageIndex] = inFlightFences[current_frame];


        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[current_frame] };
        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[current_frame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        
        // use std array here over cmdbuffers for future reference when 
        // for implementing automatic command buffer submission TODO
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
        vkResetFences(device, 1, &inFlightFences[current_frame]);

        if (vkQueueSubmit(graphics_q, 1, &submitInfo, inFlightFences[current_frame]) != VK_SUCCESS) {
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
        vkQueuePresentKHR(present_q, &presentInfo);

        vkWaitForFences(device, 1, &inFlightFences[current_frame], VK_TRUE, UINT64_MAX);
        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(gpu, format, &props);
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

        createImage(extent.width, extent.height,
            depthFormat, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            depth_image, depth_mem);

        depth_view = createImageView(depth_image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

        transitionImageLayout(depth_image, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        return VKTexture(depth_image, depth_mem, depth_view);
    }

    VKTexture loadTexture(const std::string& path) {
        int texw, texh, ch;
        auto pixels = stbi_load(path.c_str(), &texw, &texh, &ch, STBI_rgb_alpha);
        if (!pixels) {
            throw std::runtime_error("failed to laod stb image for texturing");
        }

        VkDeviceSize image_size = texw * texh * 4;
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
        vkMapMemory(device, stage_pixels_mem, 0, image_size, 0, &pdata);
        memcpy(pdata, pixels, static_cast<size_t>(image_size));
        vkUnmapMemory(device, stage_pixels_mem);

        stbi_image_free(pixels);

        VkImage texture;
        VkDeviceMemory texture_mem;

        createImage(
            texw,
            texh,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            texture,
            texture_mem
        );

        transitionImageLayout(texture, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stage_pixels, texture, static_cast<uint32_t>(texw), static_cast<uint32_t>(texh));
        transitionImageLayout(texture, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vkDestroyBuffer(device, stage_pixels, nullptr);
        vkFreeMemory(device, stage_pixels_mem, nullptr);

        auto texture_image_view = createImageView(texture, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
        
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
        samplerInfo.maxLod = 0.0f;

        VkSampler sampler;
        if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vk sampler");
        }

        return VKTexture(texture, texture_mem, texture_image_view, sampler);
    }

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }

        return imageView;
    };

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0; // TODO
        barrier.dstAccessMask = 0; // TODO

        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

            if (format == VK_FORMAT_D32_SFLOAT_S8_UINT or format == VK_FORMAT_D24_UNORM_S8_UINT) {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        }
        else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        endSingleTimeCommands(commandBuffer);
    };

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { width, height, 1 };

        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        endSingleTimeCommands(commandBuffer);
    };

    void createImage(uint32_t w, uint32_t h, VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = w;
        imageInfo.extent.height = h;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vk image");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image meory");
        }

        vkBindImageMemory(device, image, imageMemory, 0);
    };

    VKVertexBuffer createVertexBuffer(const std::vector<Vertex>& vertices) {

        VkDeviceSize buffer_size = sizeof(Vertex) * vertices.size();

        VkBuffer staging_buffer;
        VkDeviceMemory stage_mem;
        createBuffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer,
            stage_mem
        );

        void* data;
        vkMapMemory(device, stage_mem, 0, buffer_size, 0, &data);
        memcpy(data, vertices.data(), (size_t)buffer_size);
        vkUnmapMemory(device, stage_mem);

        VkBuffer vertex_buffer;
        VkDeviceMemory vertex_mem;
        createBuffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            vertex_buffer,
            vertex_mem
        );

        copyBuffer(staging_buffer, vertex_buffer, buffer_size);
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, stage_mem, nullptr);

        return VKVertexBuffer(vertex_buffer, vertex_mem);
    }

    VKIndexBuffer createIndexBuffer(const std::vector<Index>& indices) {
        VkDeviceSize indices_size = sizeof(Index) * indices.size();

        VkBuffer stage_indices_buffer;
        VkDeviceMemory indices_stage_mem;

        createBuffer(
            indices_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stage_indices_buffer,
            indices_stage_mem
        );

        void* data1;
        vkMapMemory(device, indices_stage_mem, 0, indices_size, 0, &data1);
        memcpy(data1, indices.data(), (size_t)indices_size);
        vkUnmapMemory(device, indices_stage_mem);

        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;

        createBuffer(indices_size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            indexBuffer,
            indexBufferMemory);

        copyBuffer(stage_indices_buffer, indexBuffer, indices_size);

        vkDestroyBuffer(device, stage_indices_buffer, nullptr);
        vkFreeMemory(device, indices_stage_mem, nullptr);
        
        return VKIndexBuffer(indexBuffer, indexBufferMemory, static_cast<uint32_t>(indices.size() * 3));
    }

    VkPhysicalDevice getGPU() {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
        if (device_count == 0) {
            throw std::runtime_error("failed to find any physical devices");
        }

        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

        for (const auto& device : devices) {
            VkPhysicalDeviceProperties props;
            VkPhysicalDeviceFeatures features;
            vkGetPhysicalDeviceProperties(device, &props);
            vkGetPhysicalDeviceFeatures(device, &features);
            // prefer dedicated GPU
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                return device;
            }
        }
        // else we just get the first adapter found
        // TODO: implement actual device picking and scoring
        return *devices.begin();
    }

    void recreateSwapchain(uint32_t width, uint32_t height) {

    }

    void setupSwapchain(uint32_t width, uint32_t height, const std::vector<const char*>& extensions) {
        struct SwapChainSupportDetails {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        };

        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, nullptr);

        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentModeCount, details.presentModes.data());
        }

        bool swapChainAdequate = false;
        if (extensions.empty()) {
            swapChainAdequate = !details.formats.empty() && !details.presentModes.empty();
        }

        const std::vector<VkSurfaceFormatKHR> availableFormats = details.formats;
        const std::vector<VkPresentModeKHR> availablePresentModes = details.presentModes;
        const VkSurfaceCapabilitiesKHR capabilities = details.capabilities;
        //TODO: fill the vector
        VkSurfaceFormatKHR surface_format = availableFormats[0];
        VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                surface_format = availableFormat;
            }
        }

        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                present_mode = availablePresentMode;
            }
        }

        if (capabilities.currentExtent.width != UINT32_MAX) {
            extent = capabilities.currentExtent;
        }
        else {

            VkExtent2D actualExtent = { width, height };
            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
            extent = actualExtent;
        }
        uint32_t imageCount = details.capabilities.minImageCount + 1;
        if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount) {
            imageCount = details.capabilities.maxImageCount;
        }


        VkSwapchainCreateInfoKHR sc_info = {};
        sc_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        sc_info.surface = surface;

        sc_info.minImageCount = imageCount;
        sc_info.imageFormat = surface_format.format;
        sc_info.imageColorSpace = surface_format.colorSpace;
        sc_info.imageExtent = extent;
        sc_info.imageArrayLayers = 1;
        sc_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        uint32_t queueFamilyIndices[] = { qindices.graphics.value(), qindices.present.value() };

        if (qindices.graphics != qindices.present) {
            sc_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            sc_info.queueFamilyIndexCount = 2;
            sc_info.pQueueFamilyIndices = queueFamilyIndices;
        }
        else {
            sc_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            sc_info.queueFamilyIndexCount = 0; // Optional
            sc_info.pQueueFamilyIndices = nullptr; // Optional
        }

        sc_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        sc_info.preTransform = details.capabilities.currentTransform;
        sc_info.presentMode = present_mode;
        sc_info.clipped = VK_TRUE;
        sc_info.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device, &sc_info, nullptr, &swapchain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swapchain");
        }

        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapChainImages.data());
        swapChainImageFormat = surface_format.format;

        swapChainImageViews.resize(swapChainImages.size());

        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo iv_info = {};
            iv_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            iv_info.image = swapChainImages[i];
            iv_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            iv_info.format = swapChainImageFormat;
            iv_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            iv_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            iv_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            iv_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            iv_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            iv_info.subresourceRange.baseMipLevel = 0;
            iv_info.subresourceRange.levelCount = 1;
            iv_info.subresourceRange.baseArrayLayer = 0;
            iv_info.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &iv_info, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image view");
            }
        }
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(gpu, &memProperties);
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

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    };

    VkCommandBuffer beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    };

    void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        if (vkQueueSubmit(graphics_q, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit single time queue");
        }
        vkQueueWaitIdle(graphics_q);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    };

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferCopy copyRegion = {};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    };

    void ImGuiInit(SDL_Window* window) {
        // Create Descriptor Pool
        {
            VkDescriptorPoolSize pool_sizes[] =
            {
                { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
                { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
            };
            VkDescriptorPoolCreateInfo pool_info = {};
            pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
            pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
            pool_info.pPoolSizes = pool_sizes;
            if (vkCreateDescriptorPool(device, &pool_info, nullptr, &g_DescriptorPool) != VK_SUCCESS) {
                throw std::runtime_error("failed to create descriptor pool for imgui");
            }
        }

        ImGui_ImplVulkan_InitInfo info;
        info.Device = device;
        info.PhysicalDevice = gpu;
        info.QueueFamily = qindices.graphics.value();
        info.Queue = graphics_q;
        info.PipelineCache = VK_NULL_HANDLE;
        info.DescriptorPool = g_DescriptorPool;
        info.MinImageCount = 2;
        info.ImageCount = info.MinImageCount;
        info.Allocator = nullptr;
        info.CheckVkResultFn = nullptr;
        ImGui_ImplSDL2_InitForVulkan(window);
        ImGui_ImplVulkan_Init(&info, renderPass);
    }

    void ImGuiCreateFonts() {
        VkCommandBuffer command_buffer = beginSingleTimeCommands();
        ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
        endSingleTimeCommands(command_buffer);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    void ImGuiNewFrame(SDL_Window* window) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();
    }
};

void Application::vulkan_main() {
    auto context = Raekor::PlatformContext();

    // retrieve the application settings from the config file
    serialize_settings("config.json");

    int sdl_err = SDL_Init(SDL_INIT_VIDEO);
    m_assert(sdl_err == 0, "failed to init SDL for video");

    Uint32 wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN |
        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED;

    std::vector<SDL_Rect> displays;
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        displays.push_back(SDL_Rect());
        SDL_GetDisplayBounds(i, &displays.back());
    }

    // if our display setting is higher than the nr of displays we pick the default display
    auto index = display > displays.size() - 1 ? 0 : display;
    auto window = SDL_CreateWindow(name.c_str(),
        displays[index].x,
        displays[index].y,
        displays[index].w,
        displays[index].h,
        wflags);

    // initialize ImGui
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

    Camera camera = Camera({ 0, 0, 4.0f }, 45.0f);

    // optional compilation of shaders at runtime by invoking the shader compiler's executable
    compile_shader("shaders/vulkan.vert", "shaders/vert.spv");
    compile_shader("shaders/vulkan.frag", "shaders/frag.spv");

    VKRender vk = VKRender();
    vk.init(window);
    vk.ImGuiInit(window);
    vk.ImGuiCreateFonts();

    std::puts("job well done.");

    SDL_SetWindowInputFocus(window);

    Ffilter ft_mesh = {};
    ft_mesh.name = "Supported Mesh Files";
    ft_mesh.extensions = "*.obj;*.fbx";

    bool running = true;

    // MVP uniform buffer object
    cb_vs ubo = {};

    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 rotation = { M_PI / 2, 1, 0 };
    auto rotation_quat = static_cast<glm::quat>(rotation);
    model = model * glm::toMat4(rotation_quat);

    Timer dt_timer = Timer();
    double dt = 0;
    
    //main application loop
    while (running) {
        dt_timer.start();
        //handle sdl and imgui events
        handle_sdl_gui_events({ window }, camera, dt);

        camera.update(model);
        ubo.MVP = camera.get_mvp(false);

        uint32_t frame = vk.getNextFrame();
        vk.updateUniformBuffer(ubo, frame);

        // start a new imgui frame
        vk.ImGuiNewFrame(window);

        static bool opt_fullscreen_persistant = true;
        bool opt_fullscreen = opt_fullscreen_persistant;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_DockNodeHost;
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
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        ImGui::Begin("ECS");
        if (ImGui::Button("Add Model")) {
            std::string path = context.open_file_dialog({ ft_mesh });
            if (!path.empty()) {
                // TODO: load sponza
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove Model")) {
            // TODO: remove sponza
        }
        ImGui::End();

        ImGui::ShowMetricsWindow();

        // scene panel
        ImGui::Begin("Scene");
        // toggle button for openGl vsync
        static bool use_vsync = false;
        if (ImGui::RadioButton("USE VSYNC", use_vsync)) {
            use_vsync = !use_vsync;
        }
        ImGui::End();

        ImGui::End();

        // tell imgui to collect render data
        ImGui::Render();
        // record the collected data to secondary command buffers
        vk.ImGuiRecord();
        // start the overall render pass
        vk.render(frame);
        // tell imgui we're done with the current frame
        ImGui::EndFrame();

        dt_timer.stop();
        dt = dt_timer.elapsed_ms();
    }
    vk.waitForIdle();

    /*
    vkDestroyShaderModule(vk_device, vertShaderModule, nullptr);
    vkDestroyShaderModule(vk_device, fragShaderModule, nullptr);
    vkDestroyPipelineLayout(vk_device, pipelineLayout, nullptr);
    vkDestroyBuffer(vk_device, vertex_buffer, nullptr);
    vkFreeMemory(vk_device, vertex_mem, nullptr);
    vkDestroyBuffer(vk_device, indexBuffer, nullptr);
    vkFreeMemory(vk_device, indexBufferMemory, nullptr);
    vkDestroyDescriptorPool(vk_device, descriptorPool, nullptr);
    vkDestroyImage(vk_device, texture, nullptr);
    vkFreeMemory(vk_device, texture_mem, nullptr);
    vkDestroyImageView(vk_device, texture_image_view, nullptr);
    vkDestroySampler(vk_device, sampler, nullptr);
    vkDestroyImageView(vk_device, depth_view, nullptr);
    vkDestroyImage(vk_device, depth_image, nullptr);
    vkFreeMemory(vk_device, depth_mem, nullptr);
    */
}

} // namespace Raekor