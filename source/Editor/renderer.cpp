#include "pch.h"
#include "renderer.h"
#include "Raekor/scene.h"
#include "Raekor/async.h"
#include "Raekor/camera.h"
#include "renderpass.h"

namespace Raekor {

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


GLRenderer::GLRenderer(SDL_Window* window, Viewport& viewport) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

    m_GLContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, m_GLContext);
    SDL_GL_SetSwapInterval(settings.vsync);

    // Load GL extensions using glad
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize the OpenGL context.\n";
        return;
    }

    // Loaded OpenGL successfully.
    std::cout << "OpenGL version loaded: " << GLVersion.major << "."
              << GLVersion.minor << '\n';

    if (!FileSystem::exists("assets/system/shaders/OpenGL/bin"))
        FileSystem::create_directory("assets/system/shaders/OpenGL/bin");

    const auto vulkanSDK = getenv("VULKAN_SDK");
    assert(vulkanSDK);

    for (const auto& file : FileSystem::directory_iterator("assets/system/shaders/OpenGL")) {
        if (file.is_directory()) 
            continue;

        // visual studio code glsl linter keeps compiling spirv files to the directory,
        // delete em
        if (file.path().extension() == ".spv") {
            remove(file.path().string().c_str());
            continue;
        }

        Async::sQueueJob([=]() {
            auto outfile = file.path().parent_path() / "bin" / file.path().filename();
            outfile.replace_extension(outfile.extension().string() + ".spv");

            if (!FileSystem::exists(outfile) || FileSystem::last_write_time(outfile) < file.last_write_time()) {
                auto success = GLShader::sGlslangValidator(vulkanSDK, file, outfile);

                {
                    auto lock = Async::sLock();

                    std::string result_string = success ? COUT_GREEN("finished") : COUT_RED("failed");
                    std::cout << "Compilation " << result_string << " for shader: " << file.path().string() << '\n';
                }
            }
        });
    }

    Async::sWait();

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

    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImNodes::CreateContext();
    ImGui::StyleColorsDark();
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
    m_Icons =         std::make_shared<Icons>(viewport);
    m_Bloom =         std::make_shared<Bloom>(viewport);
    m_GBuffer =       std::make_shared<GBuffer>(viewport);
    m_Tonemap =       std::make_shared<Tonemap>(viewport);
    m_Skinning =      std::make_shared<Skinning>();
    m_Voxelize =      std::make_shared<Voxelize>(512);
    m_ResolveTAA =    std::make_shared<TAAResolve>(viewport);
    m_DebugLines =    std::make_shared<DebugLines>();
    m_Atmosphere =    std::make_shared<Atmosphere>(viewport);
    m_ShadowMaps =    std::make_shared<ShadowMap>(viewport);
    m_DebugVoxels =   std::make_shared<VoxelizeDebug>(viewport);
    m_DeferredShading =  std::make_shared<DeferredShading>(viewport);

    m_RenderPasses.push_back(std::static_pointer_cast<RenderPass>(m_Icons));
    m_RenderPasses.push_back(std::static_pointer_cast<RenderPass>(m_Bloom));
    m_RenderPasses.push_back(std::static_pointer_cast<RenderPass>(m_GBuffer));
    m_RenderPasses.push_back(std::static_pointer_cast<RenderPass>(m_Tonemap));
    m_RenderPasses.push_back(std::static_pointer_cast<RenderPass>(m_Skinning));
    m_RenderPasses.push_back(std::static_pointer_cast<RenderPass>(m_Voxelize));
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


GLRenderer::~GLRenderer() {
    ImGui_ImplOpenGL3_DestroyDeviceObjects();
    SDL_GL_DeleteContext(m_GLContext);
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
}


void GLRenderer::Render(const Scene& scene, const Viewport& viewport) {
    if (!m_Timings.empty() && settings.disableTiming)
        m_Timings.clear();

    // skin all meshes in the scene
    TimeOpenGL("Skinning", [&]() {
        scene.view<const Skeleton, const Mesh>()
        .each([&](auto& animation, auto& mesh) {
            m_Skinning->compute(mesh, animation);
        });
    });

    // render 4 * 4096 cascaded shadow maps
    TimeOpenGL("Shadow cascades", [&]() {
        m_ShadowMaps->Render(viewport, scene);
    });

    // voxelize the Scene to a 3D texture
    if (settings.shouldVoxelize) {
        TimeOpenGL("Voxelize", [&]() {
            m_Voxelize->Render(scene, viewport, *m_ShadowMaps);
        });
    }

    // generate a geometry buffer with depth, normals, material and albedo
    TimeOpenGL("GBuffer", [&]() {
        m_GBuffer->Render(scene, viewport, m_FrameNr);
    });

    // render the sky using ray marching for atmospheric scattering
    TimeOpenGL("Atmosphere", [&]() {
        m_Atmosphere->computeCubemaps(viewport, scene);
    });

    // fullscreen PBR deferred shading pass
    TimeOpenGL("Deferred Shading", [&]() {
        m_DeferredShading->Render(scene, viewport, *m_ShadowMaps, *m_GBuffer, *m_Atmosphere, *m_Voxelize);
    });

    GLuint shadingResult = m_DeferredShading->result;

    if (settings.enableTAA) {
        TimeOpenGL("TAA Resolve", [&]() {
            shadingResult = m_ResolveTAA->Render(viewport, *m_GBuffer, *m_DeferredShading, m_FrameNr);
        });
    }
    else {
        // if the cvar is enabled through cvars it doesnt reset the frameNr,
        // so we just do it here
        // frameNr = 0;
    }

    // render editor icons
    TimeOpenGL("Icons", [&]() {
        m_Icons->Render(scene, viewport, shadingResult, m_GBuffer->entityTexture);
    });

    // generate downsampled bloom and do ACES tonemapping
    GLuint bloomTexture = m_DefaultBlackTexture;

    if (settings.doBloom) {
        TimeOpenGL("Bloom", [&]() {
            m_Bloom->Render(viewport, m_DeferredShading->bloomHighlights);
        });

        bloomTexture = m_Bloom->bloomTexture;
    }

    TimeOpenGL("Tonemap", [&]() {
        m_Tonemap->Render(shadingResult, bloomTexture);
    });

    if (settings.debugCascades) {
        TimeOpenGL("Debug cascade", [&]() {
            m_ShadowMaps->renderCascade(viewport, m_Tonemap->framebuffer);
        });
    }

    // render debug lines / shapes
    TimeOpenGL("Debug lines", [&]() {
        m_DebugLines->Render(viewport, m_Tonemap->result, m_GBuffer->depthTexture);
    });

    // render 3D voxel texture size ^ 3 cubes
    if (settings.debugVoxels) {
        TimeOpenGL("Debug voxels", [&]() {
            m_DebugVoxels->Render(viewport, m_Tonemap->result, *m_Voxelize);
        });
    }

    // build the imgui font texture
    if (!ImGui::GetIO().Fonts->TexID)
        ImGui_ImplOpenGL3_CreateFontsTexture();

    // bind and clear the window's swapchain
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // render ImGui
    TimeOpenGL("ImGui", [&]() {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    });

    // increment frame counter
    m_FrameNr = m_FrameNr + 1;
}



void GLRenderer::AddDebugLine(glm::vec3 p1, glm::vec3 p2) {
    m_DebugLines->points.push_back(p1);
    m_DebugLines->points.push_back(p2);
}



void GLRenderer::AddDebugBox(glm::vec3 min, glm::vec3 max, glm::mat4& m) {
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



template<typename Lambda>
void GLRenderer::TimeOpenGL(const std::string& name, Lambda&& lambda) {
    if (!settings.disableTiming) {
        m_Timings.insert({ name, std::make_unique<GLTimer>() });
        m_Timings[name]->begin();
        lambda();
        m_Timings[name]->end();
    }
    else
        lambda();
}



void GLRenderer::sUploadMeshBuffers(Mesh& mesh) {
    auto vertices = mesh.GetInterleavedVertices();

    glCreateBuffers(1, &mesh.vertexBuffer);
    glNamedBufferData(mesh.vertexBuffer, sizeof(float) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

    glCreateBuffers(1, &mesh.indexBuffer);
    glNamedBufferData(mesh.indexBuffer, sizeof(uint32_t) * mesh.indices.size(), mesh.indices.data(), GL_STATIC_DRAW);
}



void GLRenderer::sDestroyMeshBuffers(Mesh& mesh) {
    glDeleteBuffers(1, &mesh.vertexBuffer);
    glDeleteBuffers(1, &mesh.indexBuffer);
    mesh.vertexBuffer = 0, mesh.indexBuffer = 0;
}



void GLRenderer::sUploadSkeletonBuffers(Skeleton& skeleton, Mesh& mesh) {
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



void GLRenderer::sDestroySkeletonBuffers(Skeleton& skeleton) {
    glDeleteBuffers(1, &skeleton.boneIndexBuffer);
    glDeleteBuffers(1, &skeleton.boneWeightBuffer);
    glDeleteBuffers(1, &skeleton.skinnedVertexBuffer);
    glDeleteBuffers(1, &skeleton.boneTransformsBuffer);
}



void GLRenderer::sDestroyMaterialTextures(Material& material, Assets& assets) {
    glDeleteTextures(1, &material.gpuAlbedoMap);
    glDeleteTextures(1, &material.gpuNormalMap);
    glDeleteTextures(1, &material.gpuMetallicRoughnessMap);
    material.gpuAlbedoMap = 0, material.gpuNormalMap = 0, material.gpuMetallicRoughnessMap = 0;

    assets.Release(material.albedoFile);
    assets.Release(material.normalFile);
    assets.Release(material.metalroughFile);
}



void GLRenderer::sUploadMaterialTextures(Material& material, Assets& assets) {
    if (auto asset = assets.Get<TextureAsset>(material.albedoFile); asset)
        material.gpuAlbedoMap = GLRenderer::sUploadTextureFromAsset(asset, true);

    if (auto asset = assets.Get<TextureAsset>(material.normalFile); asset)
        material.gpuNormalMap = GLRenderer::sUploadTextureFromAsset(asset);

    if (auto asset = assets.Get<TextureAsset>(material.metalroughFile); asset)
        material.gpuMetallicRoughnessMap = GLRenderer::sUploadTextureFromAsset(asset);
}



GLuint GLRenderer::sUploadTextureFromAsset(const TextureAsset::Ptr& asset, bool sRGB) {
    assert(asset);
    auto dataPtr = asset->GetData();
    auto headerPtr = asset->GetHeader();

    auto texture = 0u;
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    
    auto format = sRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    glTextureStorage2D(texture, headerPtr->dwMipMapCount, format, headerPtr->dwWidth, headerPtr->dwHeight);

    for (auto mip = 0u; mip < headerPtr->dwMipMapCount; mip++) {
        const auto dimensions = glm::ivec2(std::max(headerPtr->dwWidth >> mip, 1ul), std::max(headerPtr->dwHeight >> mip, 1ul));
        const auto data_size = GLsizei(std::max(1, ((dimensions.x + 3) / 4)) * std::max(1, ((dimensions.y + 3) / 4)) * 16);
        glCompressedTextureSubImage2D(texture, mip, 0, 0, dimensions.x, dimensions.y, format, data_size, dataPtr);
        dataPtr += dimensions.x * dimensions.y;
    }

    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return texture;
}



void GLRenderer::CreateRenderTargets(const Viewport& viewport) {
    m_FrameNr = 0;

    for (auto& pass : m_RenderPasses) {
        pass->DestroyRenderTargets();
        pass->CreateRenderTargets(viewport);
    }

    m_ShadowMaps->updatePerspectiveConstants(viewport);
}

} // namespace Raekor