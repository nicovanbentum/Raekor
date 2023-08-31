#include "pch.h"
#include "renderer.h"
#include "Raekor/rmath.h"
#include "Raekor/scene.h"
#include "Raekor/async.h"
#include "Raekor/camera.h"
#include "renderpass.h"

namespace Raekor::GL {

static constexpr auto sDebugTextureNames = std::array {
    "Final",
    "Albedo",
    "Normals",
    "Material",
    "Velocity",
    "TAA Resolve",
    "Shading (Result)",
    "Bloom (Threshold)",
    "Bloom (Blur 1)",
    "Bloom (Final)",
};

void MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
    if (severity != GL_DEBUG_SEVERITY_NOTIFICATION) {
        fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
                (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
                type, severity, message);

        switch (id) {
            case 131218: return; // shader state recompilation
            default: {
                __debugbreak();
            }
        }
    }
}


Renderer::Renderer(SDL_Window* window, Viewport& viewport) : 
    IRenderer(GraphicsAPI::OpenGL4_6),
    m_Window(window) 
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

    m_GLContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, m_GLContext);
    SDL_GL_SetSwapInterval(mSettings.vsync);

    // Load GL extensions using glad
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize the OpenGL context.\n";
        return;
    }

    // Loaded OpenGL successfully.
    std::cout << "[Renderer] OpenGL version loaded: " << GLVersion.major << "."
              << GLVersion.minor << '\n';

    GPUInfo gpu_info = {};
    gpu_info.mVendor = (const char*)glGetString(GL_VENDOR);
    gpu_info.mProduct = (const char*)glGetString(GL_RENDERER);
    gpu_info.mActiveAPI = std::string("OpenGL " + std::string((const char*)glGetString(GL_VERSION)));
    SetGPUInfo(gpu_info);

    if (!fs::exists("assets/system/shaders/OpenGL/bin"))
        fs::create_directory("assets/system/shaders/OpenGL/bin");

    const auto vulkanSDK = getenv("VULKAN_SDK");
    assert(vulkanSDK);

    for (const auto& file : fs::directory_iterator("assets/system/shaders/OpenGL")) {
        if (file.is_directory()) 
            continue;

        // visual studio code glsl linter keeps compiling spirv files to the directory,
        // delete em
        if (file.path().extension() == ".spv") {
            remove(file.path().string().c_str());
            continue;
        }

        g_ThreadPool.QueueJob([=]() {
            auto outfile = file.path().parent_path() / "bin" / file.path().filename();
            outfile.replace_extension(outfile.extension().string() + ".spv");

            if (!fs::exists(outfile) || fs::last_write_time(outfile) < file.last_write_time()) {
                auto success = GLShader::sGlslangValidator(vulkanSDK, file, outfile);

                {
                    auto lock = std::scoped_lock(g_ThreadPool.GetMutex());

                    std::string result_string = success ? COUT_GREEN("finished") : COUT_RED("failed");
                    std::cout << "Compilation " << result_string << " for shader: " << file.path().string() << '\n';
                }
            }
        });
    }

    g_ThreadPool.WaitForJobs();

    // set debug callback
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(MessageCallback, nullptr);

    // culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // winding order and cube maps
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glFrontFace(GL_CCW);

    // blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    unsigned int vertexArrayID;
    glGenVertexArrays(1, &vertexArrayID);
    glBindVertexArray(vertexArrayID);

    ImGui_ImplSDL2_InitForOpenGL(window, &m_GLContext);
    ImGui_ImplOpenGL3_Init("#version 450");
    ImGui_ImplOpenGL3_CreateDeviceObjects();

    // get GUI i/o and set a bunch of settings
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    // io.ConfigDockingWithShift = true;

    // initialise all the render passes
    viewport.SetSize(UVec2(2560, 1440));

    m_Icons             = std::make_shared<Icons>(viewport);
    m_Bloom             = std::make_shared<Bloom>(viewport);
    m_GBuffer           = std::make_shared<GBuffer>(viewport);
    m_Tonemap           = std::make_shared<Tonemap>(viewport);
    m_Skinning          = std::make_shared<Skinning>();
    m_Voxelize          = std::make_shared<Voxelize>(512);
    m_ImGuiPass         = std::make_shared<ImGuiPass>();
    m_ResolveTAA        = std::make_shared<TAAResolve>(viewport);
    m_DebugLines        = std::make_shared<DebugLines>();
    m_Atmosphere        = std::make_shared<Atmosphere>(viewport);
    m_ShadowMaps        = std::make_shared<ShadowMap>(viewport);
    m_DebugVoxels       = std::make_shared<VoxelizeDebug>(viewport);
    m_DeferredShading   = std::make_shared<DeferredShading>(viewport);

    m_RenderPasses.push_back(std::static_pointer_cast<RenderPass>(m_Icons));
    m_RenderPasses.push_back(std::static_pointer_cast<RenderPass>(m_Bloom));
    m_RenderPasses.push_back(std::static_pointer_cast<RenderPass>(m_GBuffer));
    m_RenderPasses.push_back(std::static_pointer_cast<RenderPass>(m_Tonemap));
    m_RenderPasses.push_back(std::static_pointer_cast<RenderPass>(m_Skinning));
    m_RenderPasses.push_back(std::static_pointer_cast<RenderPass>(m_Voxelize));
    m_RenderPasses.push_back(std::static_pointer_cast<RenderPass>(m_ImGuiPass));
    m_RenderPasses.push_back(std::static_pointer_cast<RenderPass>(m_ResolveTAA));
    m_RenderPasses.push_back(std::static_pointer_cast<RenderPass>(m_DebugLines));
    m_RenderPasses.push_back(std::static_pointer_cast<RenderPass>(m_Atmosphere));
    m_RenderPasses.push_back(std::static_pointer_cast<RenderPass>(m_ShadowMaps));
    m_RenderPasses.push_back(std::static_pointer_cast<RenderPass>(m_DebugVoxels));
    m_RenderPasses.push_back(std::static_pointer_cast<RenderPass>(m_DeferredShading));

    auto setDefaultTextureParams = [](GLuint& texture) {
        glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTextureParameteri(texture, GL_TEXTURE_WRAP_R, GL_REPEAT);
    };

    glCreateTextures(GL_TEXTURE_2D, 1, &m_DefaultBlackTexture);
    glTextureStorage2D(m_DefaultBlackTexture, 1, GL_RGBA8, 1, 1);
    setDefaultTextureParams(m_DefaultBlackTexture);

    auto createDefaultMaterialTexture = [](GLuint& texture, const glm::vec4& value) {
        glCreateTextures(GL_TEXTURE_2D, 1, &texture);
        glTextureStorage2D(texture, 1, GL_RGBA16F, 1, 1);
        glTextureSubImage2D(texture, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, glm::value_ptr(value));
    };

    createDefaultMaterialTexture(Material::Default.gpuAlbedoMap, glm::vec4(1.0f));
    setDefaultTextureParams(Material::Default.gpuAlbedoMap);

    createDefaultMaterialTexture(Material::Default.gpuMetallicRoughnessMap, glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));
    setDefaultTextureParams(Material::Default.gpuMetallicRoughnessMap);

    createDefaultMaterialTexture(Material::Default.gpuNormalMap, glm::vec4(0.5f, 0.5f, 1.0f, 1.0f));
    setDefaultTextureParams(Material::Default.gpuNormalMap);
}


Renderer::~Renderer() {
    ImGui_ImplOpenGL3_DestroyDeviceObjects();
    SDL_GL_DeleteContext(m_GLContext);
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
}


void Renderer::Render(const Scene& scene, const Viewport& viewport) {
    // skin all meshes in the scene
    {
        const auto timer = RenderPass::ScopedTimer("Skinning", m_Skinning.get(), !mSettings.disableTiming);
        for (const auto& [entity, mesh, skeleton] : scene.Each<Mesh, Skeleton>())
            m_Skinning->compute(mesh, skeleton);
    }

    // render 4 * 4096 cascaded shadow maps
    {
        const auto timer = RenderPass::ScopedTimer("Shadow Cascades", m_ShadowMaps.get(), !mSettings.disableTiming);
       m_ShadowMaps->Render(viewport, scene);
    }

    // voxelize the Scene to a 3D texture
    if (mSettings.shouldVoxelize) {
        const auto timer = RenderPass::ScopedTimer("Voxelize", m_Voxelize.get(), !mSettings.disableTiming);
        m_Voxelize->Render(scene, viewport, *m_ShadowMaps);
    }

    // generate a geometry buffer with depth, normals, material and albedo
    {
        const auto timer = RenderPass::ScopedTimer("GBuffer", m_GBuffer.get(), !mSettings.disableTiming);
        m_GBuffer->Render(scene, viewport, m_FrameNr);
    }
    
    // render the sky using ray marching for atmospheric scattering
    {
        const auto timer = RenderPass::ScopedTimer("Atmosphere", m_Atmosphere.get(), !mSettings.disableTiming);
        m_Atmosphere->computeCubemaps(viewport, scene);
    }

    // fullscreen PBR deferred shading pass
    {
        const auto timer = RenderPass::ScopedTimer("Deferred Shading", m_DeferredShading.get(), !mSettings.disableTiming);
        m_DeferredShading->Render(scene, viewport, *m_ShadowMaps, *m_GBuffer, *m_Atmosphere, *m_Voxelize);
    }

    auto shading_result = m_DeferredShading->result;

    if (mSettings.enableTAA) {
        const auto timer = RenderPass::ScopedTimer("TAA Resolve", m_ResolveTAA.get(), !mSettings.disableTiming);
        shading_result = m_ResolveTAA->Render(viewport, *m_GBuffer, *m_DeferredShading, m_FrameNr);
    }
    else {
        // if the cvar is enabled through cvars it doesnt reset the frameNr,
        // so we just do it here
        // frameNr = 0;
    }

    // render editor icons
    {
        const auto timer = RenderPass::ScopedTimer("Icons", m_Icons.get(), !mSettings.disableTiming);
        m_Icons->Render(scene, viewport, shading_result, m_GBuffer->entityTexture);
    }

    // generate downsampled bloom and do ACES tonemapping
    auto bloomTexture = m_DefaultBlackTexture;

    if (mSettings.doBloom) {
        const auto timer = RenderPass::ScopedTimer("Bloom", m_Bloom.get(), !mSettings.disableTiming);
        m_Bloom->Render(viewport, m_DeferredShading->bloomHighlights);
        bloomTexture = m_Bloom->bloomTexture;
    }

    {
        const auto timer = RenderPass::ScopedTimer("Tonemap", m_Tonemap.get(), !mSettings.disableTiming);
        m_Tonemap->Render(shading_result, bloomTexture);
    }

    if (mSettings.debugCascades)
        m_ShadowMaps->renderCascade(viewport, m_Tonemap->framebuffer);

    // render debug lines / shapes
    {
        const auto timer = RenderPass::ScopedTimer("Debug Primitives", m_DebugLines.get(), !mSettings.disableTiming);
        m_DebugLines->Render(viewport, m_Tonemap->result, m_GBuffer->depthTexture);
    }

    // render 3D voxel texture size ^ 3 cubes
    if (mSettings.debugVoxels) {
        const auto timer = RenderPass::ScopedTimer("Debug Voxels", m_DebugVoxels.get(), !mSettings.disableTiming);
        m_DebugVoxels->Render(viewport, m_Tonemap->result, *m_Voxelize);
    }

    // build the imgui font texture
    if (!ImGui::GetIO().Fonts->TexID)
        ImGui_ImplOpenGL3_CreateFontsTexture();

    // bind and clear the window's swapchain
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // render ImGui
    {
        const auto timer = RenderPass::ScopedTimer("ImGui", m_ImGuiPass.get(), !mSettings.disableTiming);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    // increment frame counter
    m_FrameNr = m_FrameNr + 1;

    // swap the backbuffer
    assert(m_Window);
    SDL_GL_SwapWindow(m_Window);
}



void Renderer::AddDebugLine(Vec3 p1, Vec3 p2) {
    m_DebugLines->points.push_back(p1);
    m_DebugLines->points.push_back(p2);
}



void Renderer::AddDebugBox(Vec3 min, Vec3 max, const Mat4x4& m) {
    AddDebugLine(glm::vec3(m * glm::vec4(min.x, min.y, min.z, 1.0)), glm::vec3(m * glm::vec4(max.x, min.y, min.z, 1.0f)));
    AddDebugLine(glm::vec3(m * glm::vec4(max.x, min.y, min.z, 1.0)), glm::vec3(m * glm::vec4(max.x, max.y, min.z, 1.0f)));
    AddDebugLine(glm::vec3(m * glm::vec4(max.x, max.y, min.z, 1.0)), glm::vec3(m * glm::vec4(min.x, max.y, min.z, 1.0f)));
    AddDebugLine(glm::vec3(m * glm::vec4(min.x, max.y, min.z, 1.0)), glm::vec3(m * glm::vec4(min.x, min.y, min.z, 1.0f)));
    AddDebugLine(glm::vec3(m * glm::vec4(min.x, min.y, min.z, 1.0)), glm::vec3(m * glm::vec4(min.x, min.y, max.z, 1.0f)));
    AddDebugLine(glm::vec3(m * glm::vec4(max.x, min.y, min.z, 1.0)), glm::vec3(m * glm::vec4(max.x, min.y, max.z, 1.0f)));
    AddDebugLine(glm::vec3(m * glm::vec4(max.x, max.y, min.z, 1.0)), glm::vec3(m * glm::vec4(max.x, max.y, max.z, 1.0f)));
    AddDebugLine(glm::vec3(m * glm::vec4(min.x, max.y, min.z, 1.0)), glm::vec3(m * glm::vec4(min.x, max.y, max.z, 1.0f)));
    AddDebugLine(glm::vec3(m * glm::vec4(min.x, min.y, max.z, 1.0)), glm::vec3(m * glm::vec4(max.x, min.y, max.z, 1.0f)));
    AddDebugLine(glm::vec3(m * glm::vec4(max.x, min.y, max.z, 1.0)), glm::vec3(m * glm::vec4(max.x, max.y, max.z, 1.0f)));
    AddDebugLine(glm::vec3(m * glm::vec4(max.x, max.y, max.z, 1.0)), glm::vec3(m * glm::vec4(min.x, max.y, max.z, 1.0f)));
    AddDebugLine(glm::vec3(m * glm::vec4(min.x, max.y, max.z, 1.0)), glm::vec3(m * glm::vec4(min.x, min.y, max.z, 1.0f)));
}



uint64_t Renderer::GetDisplayTexture() { 
    const auto targets = std::array {
        m_Tonemap->result,
        m_GBuffer->albedoTexture,
        m_GBuffer->normalTexture,
        m_GBuffer->materialTexture,
        m_GBuffer->velocityTexture,
        m_ResolveTAA->resultBuffer,
        m_DeferredShading->result,
        m_DeferredShading->bloomHighlights,
        m_Bloom->blurTexture,
        m_Bloom->bloomTexture,
    };

    assert(targets.size() == sDebugTextureNames.size());
    return targets[mSettings.mDebugTexture]; 
}



void Renderer::DrawImGui(Scene& inScene, const Viewport& inViewport) {
    if (ImGui::Checkbox("VSync", (bool*)(&mSettings.vsync)))
        SDL_GL_SetSwapInterval(mSettings.vsync);

    ImGui::SameLine();

    if (ImGui::Checkbox("TAA", (bool*)(&mSettings.enableTAA)))
        m_FrameNr = 0;

    ImGui::NewLine();
    ImGui::Text("VCTGI");
    ImGui::Separator();

    ImGui::DragFloat("Radius", &m_Voxelize->worldSize, 0.05f, 1.0f, FLT_MAX, "%.2f");

    ImGui::NewLine();
    ImGui::Text("CSM");
    ImGui::Separator();

    if (ImGui::DragFloat("Bias constant", &m_ShadowMaps->settings.depthBiasConstant, 0.01f, 0.0f, FLT_MAX, "%.2f")) {}
    if (ImGui::DragFloat("Bias slope factor", &m_ShadowMaps->settings.depthBiasSlope, 0.01f, 0.0f, FLT_MAX, "%.2f")) {}
    if (ImGui::DragFloat("Split lambda", &m_ShadowMaps->settings.cascadeSplitLambda, 0.0001f, 0.0f, 1.0f, "%.4f")) {
        m_ShadowMaps->updatePerspectiveConstants(inViewport);
    }

    ImGui::NewLine();
    ImGui::Text("Bloom");
    ImGui::Separator();
    ImGui::DragFloat3("Threshold", glm::value_ptr(m_DeferredShading->settings.bloomThreshold), 0.01f, 0.0f, 10.0f, "%.3f");

    for (const auto& render_pass : m_RenderPasses)
        if (render_pass->GetName())
            ImGui::Text("%s : %.3f ms", render_pass->GetName(), render_pass->GetElapsedTime());
}



void Renderer::UploadMeshBuffers(Mesh& mesh) {
    auto vertices = mesh.GetInterleavedVertices();

    glCreateBuffers(1, &mesh.vertexBuffer);
    glNamedBufferData(mesh.vertexBuffer, sizeof(float) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

    glCreateBuffers(1, &mesh.indexBuffer);
    glNamedBufferData(mesh.indexBuffer, sizeof(uint32_t) * mesh.indices.size(), mesh.indices.data(), GL_STATIC_DRAW);
}



void Renderer::DestroyMeshBuffers(Mesh& mesh) {
    glDeleteBuffers(1, &mesh.vertexBuffer);
    glDeleteBuffers(1, &mesh.indexBuffer);
    mesh.vertexBuffer = 0, mesh.indexBuffer = 0;
}



void Renderer::UploadSkeletonBuffers(Skeleton& skeleton, Mesh& mesh) {
    glCreateBuffers(1, &skeleton.boneIndexBuffer);
    glNamedBufferData(skeleton.boneIndexBuffer, skeleton.boneIndices.size() * sizeof(glm::ivec4), skeleton.boneIndices.data(), GL_STATIC_COPY);

    glCreateBuffers(1, &skeleton.boneWeightBuffer);
    glNamedBufferData(skeleton.boneWeightBuffer, skeleton.boneWeights.size() * sizeof(glm::vec4), skeleton.boneWeights.data(), GL_STATIC_COPY);

    glCreateBuffers(1, &skeleton.boneTransformsBuffer);
    glNamedBufferData(skeleton.boneTransformsBuffer, skeleton.boneTransformMatrices.size() * sizeof(glm::mat4), skeleton.boneTransformMatrices.data(), GL_DYNAMIC_READ);

    auto originalMeshBuffer = mesh.GetInterleavedVertices();

    if (skeleton.skinnedVertexBuffer) glDeleteBuffers(1, &skeleton.skinnedVertexBuffer);
    glCreateBuffers(1, &skeleton.skinnedVertexBuffer);
    glNamedBufferData(skeleton.skinnedVertexBuffer, sizeof(originalMeshBuffer[0]) * originalMeshBuffer.size(), originalMeshBuffer.data(), GL_STATIC_DRAW);
}



void Renderer::DestroySkeletonBuffers(Skeleton& skeleton) {
    glDeleteBuffers(1, &skeleton.boneIndexBuffer);
    glDeleteBuffers(1, &skeleton.boneWeightBuffer);
    glDeleteBuffers(1, &skeleton.skinnedVertexBuffer);
    glDeleteBuffers(1, &skeleton.boneTransformsBuffer);
}



void Renderer::DestroyMaterialTextures(Material& material, Assets& assets) {
    glDeleteTextures(1, &material.gpuAlbedoMap);
    glDeleteTextures(1, &material.gpuNormalMap);
    glDeleteTextures(1, &material.gpuMetallicRoughnessMap);
    material.gpuAlbedoMap = 0, material.gpuNormalMap = 0, material.gpuMetallicRoughnessMap = 0;

    assets.Release(material.albedoFile);
    assets.Release(material.normalFile);
    assets.Release(material.metalroughFile);
}



GLuint Renderer::UploadTextureFromAsset(const TextureAsset::Ptr& asset, bool sRGB) {
    auto dataPtr = asset->GetData();
    auto headerPtr = asset->GetHeader();

    auto alpha = headerPtr->ddspf.dwABitMask != 0;
    auto gl_format = sRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    auto dds_format = (EDDSFormat)headerPtr->ddspf.dwFourCC;
    
    switch (dds_format) {
        case EDDSFormat::DDS_FORMAT_DXT1: gl_format = sRGB ? alpha ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT : GL_COMPRESSED_SRGB_S3TC_DXT1_EXT : alpha ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT : GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
            break;
        case EDDSFormat::DDS_FORMAT_DXT3: gl_format = sRGB ? alpha ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT : NULL                             : alpha ? GL_COMPRESSED_RGBA_S3TC_DXT3_EXT : NULL;
            break;
        case EDDSFormat::DDS_FORMAT_DXT5: gl_format = sRGB ? alpha ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT : alpha ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT : NULL;
            break;
        case EDDSFormat::DDS_FORMAT_ATI2:
            gl_format = GL_COMPRESSED_RED_GREEN_RGTC2_EXT;
            break;
        case EDDSFormat::DDS_FORMAT_ATI1:
            gl_format = GL_COMPRESSED_RED_RGTC1_EXT;
            break;
        case EDDSFormat::DDS_FORMAT_DXT2:
        case EDDSFormat::DDS_FORMAT_DXT4:

        default:
        assert(false);
    }

    assert(dds_format != NULL);

    auto texture = 0u;
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    
    glTextureStorage2D(texture, headerPtr->dwMipMapCount, gl_format, headerPtr->dwWidth, headerPtr->dwHeight);

    auto block_size = dds_format == DDS_FORMAT_DXT1 ? 8 : 16;

    for (auto mip = 0u; mip < headerPtr->dwMipMapCount; mip++) {
        const auto dimensions = glm::ivec2(std::max(headerPtr->dwWidth >> mip, 1ul), std::max(headerPtr->dwHeight >> mip, 1ul));
        const auto data_size = GLsizei(std::max(1, ((dimensions.x + 3) / 4)) * std::max(1, ((dimensions.y + 3) / 4)) * block_size);
        glCompressedTextureSubImage2D(texture, mip, 0, 0, dimensions.x, dimensions.y, gl_format, data_size, dataPtr);
        dataPtr += dimensions.x * dimensions.y;
    }

    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return texture;
}



uint32_t Renderer::GetDebugTextureCount() const {
    return sDebugTextureNames.size();
}

const char* Renderer::GetDebugTextureName(uint32_t inIndex) const {
    assert(inIndex < sDebugTextureNames.size());
    return sDebugTextureNames[inIndex];
}



uint32_t Renderer::GetScreenshotBuffer(uint8_t* ioBuffer) {
    int width = 0, height = 0;
    glGetTextureLevelParameteriv(GetDisplayTexture(), 0, GL_TEXTURE_WIDTH, &width);
    glGetTextureLevelParameteriv(GetDisplayTexture(), 0, GL_TEXTURE_HEIGHT, &height);
    
    const auto buffer_size = width * height * 4 * sizeof(uint8_t);

    if (ioBuffer)
        glGetTextureImage(GetDisplayTexture(), 0, GL_RGBA, GL_UNSIGNED_BYTE, width * height * 4 * sizeof(uint8_t), ioBuffer);

    return buffer_size;
}


void Renderer::CreateRenderTargets(const Viewport& viewport) {
    m_FrameNr = 0;

    for (auto& pass : m_RenderPasses) {
        pass->DestroyRenderTargets();
        pass->CreateRenderTargets(viewport);
    }

    m_ShadowMaps->updatePerspectiveConstants(viewport);
}

} // namespace Raekor