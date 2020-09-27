#include "pch.h"
#include "VKRenderer.h"
#include "mesh.h"

namespace Raekor {
namespace VK {

    void Renderer::recordModel() {
        for (int i = 0; i < meshes.size(); i++) {
            recordMeshBuffer(i, meshes[i], pipelineLayout, *modelSet, secondaryBuffers[meshes.size() - 1 - i]);
        }
    }

    void Renderer::reloadShaders() {
        // wait for the device to idle , since it's an engine function we dont care about performance
        if (vkDeviceWaitIdle(context.device) != VK_SUCCESS) {
            throw std::runtime_error("failed to wait for the gpu to idle");
        }
        // recompile and reload the shaders
        VK::Shader::compileFromCommandLine("shaders/Vulkan/vulkan.vert", "shaders/Vulkan/vert.spv");
        VK::Shader::compileFromCommandLine("shaders/Vulkan/vulkan.frag", "shaders/Vulkan/frag.spv");
        vert.reload();
        frag.reload();
        std::array<VkPipelineShaderStageCreateInfo, 2> shaders = {
            vert.getInfo(VK_SHADER_STAGE_VERTEX_BIT),
            frag.getInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
        };

        // recreate the graphics pipeline and re-record the mesh buffers
        // TODO: this still uses the depth order hack
        createGraphicsPipeline(shaders);
        recordModel();
    }

    Renderer::~Renderer() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        // destroy/free pipeline related stuff
        vkDestroyPipeline(context.device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);

        // destroy/free command buffer stuff
        std::array<VkCommandBuffer, 2> buffers = { maincmdbuffer, imguicmdbuffer};
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

        swapchain.destroy(context.device);
  
        // destroy memory objects
        modelUbo->destroy(bufferAllocator);

        vmaDestroyBuffer(bufferAllocator, vertexBuffer, vertexBufferAlloc);
        vmaDestroyBuffer(bufferAllocator, indexBuffer, indexBufferAlloc);

        for (auto& texture : textures) {
            texture.destroy(bufferAllocator);
        }

        depth_texture->destroy(bufferAllocator);

        vmaDestroyAllocator(bufferAllocator);
    }

    
    uint32_t Renderer::getMeshCount() {
        return static_cast<uint32_t>(meshes.size());
    }

    Renderer::Renderer(SDL_Window* window)
        : context(window),
        vert(context, "shaders/Vulkan/vert.spv"),
        frag(context, "shaders/Vulkan/frag.spv")
    {
        VmaAllocatorCreateInfo allocInfo = {};
        allocInfo.physicalDevice = context.PDevice;
        allocInfo.device = context.device;
        allocInfo.instance = context.instance;
        allocInfo.vulkanApiVersion = VK_API_VERSION_1_2;

        VkResult vmaResult = vmaCreateAllocator(&allocInfo, &bufferAllocator);
        if (vmaResult != VK_SUCCESS) {
            throw std::runtime_error("failed create vma allocator");
        }

        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        VkPresentModeKHR mode = vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
        if (!swapchain.create(context, { w,h }, mode)) {
            std::puts("wtfffffffffffffffff");
        }

        initResources();
        setupModelStageUniformBuffers();

        std::array<VkPipelineShaderStageCreateInfo, 2> shaders = {
            vert.getInfo(VK_SHADER_STAGE_VERTEX_BIT),
            frag.getInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
        };

        // create the graphics pipeline
        createGraphicsPipeline(shaders);
        setupFrameBuffers();

        // allocate and record the command buffers for imgui, skybox and scene
        allocateCommandBuffers();
        recordModel();

        // setup sysnc objects for synchronization between cpu and gpu
        setupSyncObjects();
    }

    void Renderer::destroyGraphicsPipeline() {
        vkDestroyPipeline(context.device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);
        vkDestroyRenderPass(context.device, renderPass, nullptr);
    }

    void Renderer::initResources() {
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
        std::vector<Triangle> indices;

        for (unsigned int m = 0, ti = 0; m < scene->mNumMeshes; m++) {
            auto ai_mesh = scene->mMeshes[m];

            VKMesh mm;
            mm.indexOffset = static_cast<uint32_t>(indices.size() * 3);
            mm.indexRange = ai_mesh->mNumFaces * 3;
            mm.vertexOffset = static_cast<uint32_t>(vertices.size());
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

        {   // vertex buffer upload
            const size_t sizeInBytes = sizeof(Vertex) * vertices.size();
            auto [stagingBuffer, stagingAlloc, stagingBufferAllocInfo] = context.device.createStagingBuffer(bufferAllocator, sizeInBytes);

            // copy the data over
            memcpy(stagingBufferAllocInfo.pMappedData, vertices.data(), sizeInBytes);

            VkBufferCreateInfo vbInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
            vbInfo.size = sizeInBytes;
            vbInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            
            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            allocInfo.flags = NULL;

            auto vkresult = vmaCreateBuffer(bufferAllocator, &vbInfo, &allocInfo, &vertexBuffer, &vertexBufferAlloc, &vertexBufferAllocInfo);
            assert(vkresult == VK_SUCCESS);

            context.device.copyBuffer(stagingBuffer, vertexBuffer, sizeInBytes);

            vmaDestroyBuffer(bufferAllocator, stagingBuffer, stagingAlloc);
        }

        {   // index buffer upload
            const size_t sizeInBytes = sizeof(Triangle) * indices.size();
            auto [stagingBuffer, stagingAlloc, stagingBufferAllocInfo] = context.device.createStagingBuffer(bufferAllocator, sizeInBytes);
            
            // copy the data over
            memcpy(stagingBufferAllocInfo.pMappedData, indices.data(), sizeInBytes);

            VkBufferCreateInfo vbInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
            vbInfo.size = sizeInBytes;
            vbInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            allocInfo.flags = NULL;

            auto vkresult = vmaCreateBuffer(bufferAllocator, &vbInfo, &allocInfo, &indexBuffer, &indexBufferAlloc, &indexBufferAllocInfo);
            assert(vkresult == VK_SUCCESS);

            context.device.copyBuffer(stagingBuffer, indexBuffer, sizeInBytes);

            vmaDestroyBuffer(bufferAllocator, stagingBuffer, stagingAlloc);
        }

        for (unsigned int m = 0, ti = 0; m < scene->mNumMeshes; m++) {
            auto ai_mesh = scene->mMeshes[m];

            std::string texture_path;
            auto material = scene->mMaterials[ai_mesh->mMaterialIndex];
            if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
                aiString filename;
                material->GetTexture(aiTextureType_DIFFUSE, 0, &filename);
                texture_path = parseFilepath(path, PATH_OPTIONS::DIR) + std::string(filename.C_Str());
            }

            if (!texture_path.empty()) {
                if (seen.find(texture_path) == seen.end()) {
                    meshes[m].textureIndex = ti;
                    seen[texture_path] = ti;
                    ti++;
                }
                else meshes[m].textureIndex = seen[texture_path];
            }
            else meshes[m].textureIndex = -1;
        }


        std::vector<Stb::Image> images = std::vector<Stb::Image>(seen.size());
        for (auto& image : images) {
            image.format = RGBA;
        }

        std::vector<std::future<void>> futures;
        for (const auto& kv : seen) {
            futures.push_back(std::async(std::launch::async, &Stb::Image::load, &images[kv.second], kv.first, true));
        }

        for (auto& future : futures) {
            future.wait();
        }

        textures.reserve(images.size());
        for (const auto& image : images) {
            textures.emplace_back(context, image, bufferAllocator);
        }

        VkVertexInputAttributeDescription pos = {};
        pos.binding = 0;
        pos.location = 0;
        pos.format = VK_FORMAT_R32G32B32_SFLOAT;
        pos.offset = 0;

        VkVertexInputAttributeDescription uv = {};
        uv.binding = 0;
        uv.location = 1;
        uv.format = VK_FORMAT_R32G32_SFLOAT;
        uv.offset = pos.offset + sizeof(float) * 3;

        VkVertexInputAttributeDescription normal = {};
        normal.binding = 0;
        normal.location = 2;
        normal.format = VK_FORMAT_R32G32B32_SFLOAT;
        normal.offset = uv.offset + sizeof(float) * 2;

        layout.push_back(pos);
        layout.push_back(uv);
        layout.push_back(normal);

        bindingDescription = {};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        input_state = {};
        input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        input_state.vertexBindingDescriptionCount = 1;
        input_state.pVertexBindingDescriptions = &bindingDescription;
        input_state.vertexAttributeDescriptionCount = static_cast<uint32_t>(layout.size());
        input_state.pVertexAttributeDescriptions = layout.data();

        std::cout << "mesh total = " << meshes.size() << "\n";
    }

    void Renderer::setupModelStageUniformBuffers() {
        // init model set
        modelSet.reset(new VK::DescriptorSet(context));

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(context.PDevice, &props);

        // Calculate required alignment based on minimum device offset alignment
        const size_t minUboAlignment = props.limits.minUniformBufferOffsetAlignment;
        dynamicAlignment = sizeof(MVP);
        if (minUboAlignment > 0) {
            dynamicAlignment = (dynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
        }

        // whole size to allocate entire buffer
        const auto wholeSize = dynamicAlignment * meshes.size();

        // allocate the CPU side data
        uboDynamic.mvp = (MVP*)malloc(wholeSize);
        memset(uboDynamic.mvp, 1, wholeSize);

        // init and update the GPU buffer
        modelUbo.reset(new VK::UniformBuffer(context, bufferAllocator, wholeSize));
        modelUbo->update(bufferAllocator, uboDynamic.mvp, wholeSize);

        // bind and complete set
        modelSet->bind(0, *modelUbo, VK_SHADER_STAGE_VERTEX_BIT);
        modelSet->bind(1, textures, VK_SHADER_STAGE_FRAGMENT_BIT);
        modelSet->complete(context);
    }

    void Renderer::setupFrameBuffers() {
        if(depth_texture) depth_texture->destroy(bufferAllocator);
        depth_texture.reset(new VK::DepthTexture(context, { swapchain.extent.width, swapchain.extent.height }, bufferAllocator));
        swapchain.setupFrameBuffers(context, renderPass, { depth_texture->view });
    }

    void Renderer::setupSyncObjects() {
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

    void Renderer::allocateCommandBuffers() {
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
    }

    uint32_t Renderer::getNextFrame() {
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

    void Renderer::waitForIdle() {
        if (vkDeviceWaitIdle(context.device) != VK_SUCCESS) {
            throw std::runtime_error("failed to wait for the gpu to idle");
        }
    }

    void Renderer::recordMeshBuffer(uint32_t bufferIndex, VKMesh& m, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSets, VkCommandBuffer& cmdbuffer) {
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
        vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(cmdbuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        uint32_t offset = static_cast<uint32_t>(bufferIndex * dynamicAlignment);
        vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout, 0, 1, &descriptorSets, 1, &offset);

        vkCmdDrawIndexed(cmdbuffer, m.indexRange, 1, m.indexOffset, m.vertexOffset, 0);
        if (vkEndCommandBuffer(cmdbuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to end command buffer");
        }
    }

    void Renderer::createGraphicsPipeline(std::array<VkPipelineShaderStageCreateInfo, 2> shaders) {
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
        pipelineLayoutInfo.pSetLayouts = &modelSet->layout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pcr;

        if (vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
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
        depthAttachment.format = context.PDevice.findSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
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
    }

    void Renderer::ImGuiRecord() {
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

    void Renderer::render(uint32_t imageIndex, const glm::mat4& sky_transform) {
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
        clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
        clearValues[1].depthStencil = { 1.0f, 0 };

        render_info.clearValueCount = static_cast<uint32_t>(clearValues.size());
        render_info.pClearValues = clearValues.data();

        if (vkBeginCommandBuffer(maincmdbuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        }

        // start the render pass and execute secondary command buffers
        vkCmdBeginRenderPass(maincmdbuffer, &render_info, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

        modelUbo->update(bufferAllocator, uboDynamic.mvp);

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

    void Renderer::recreateSwapchain(bool useVsync) {
        vsync = useVsync;
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
        VkPresentModeKHR mode = useVsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
        swapchain.destroy(context.device);
        if (!swapchain.create(context, { w, h }, mode)) {
            std::puts("WTF SWAP CHAIN FAILED??");
        }

        // re-create the graphics pipeline
        destroyGraphicsPipeline();
        std::array<VkPipelineShaderStageCreateInfo, 2> shaders = {
            vert.getInfo(VK_SHADER_STAGE_VERTEX_BIT),
            frag.getInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
        };

        createGraphicsPipeline(shaders);
        setupFrameBuffers();
        recordModel();
    }

    void Renderer::ImGuiInit(SDL_Window* window) {
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

    void Renderer::ImGuiCreateFonts() {
        VkCommandBuffer command_buffer = context.device.beginSingleTimeCommands();
        ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
        context.device.endSingleTimeCommands(command_buffer);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    void Renderer::ImGuiNewFrame(SDL_Window* window) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();
    }

} // vk
} // raekor