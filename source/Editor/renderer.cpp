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
                //breakpoint
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

    context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(settings.vsync);

    // Load GL extensions using glad
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize the OpenGL context.\n";
        return;
    }

    // Loaded OpenGL successfully.
    std::cout << "OpenGL version loaded: " << GLVersion.major << "."
              << GLVersion.minor << '\n';

    if (!fs::exists("assets/system/shaders/OpenGL/bin")) {
        fs::create_directory("assets/system/shaders/OpenGL/bin");
    }

    const auto vulkanSDK = getenv("VULKAN_SDK");
    assert(vulkanSDK);

    for (const auto& file : fs::directory_iterator("assets/system/shaders/OpenGL")) {
        if (file.is_directory()) continue;

        // visual studio code glsl linter keeps compiling spirv files to the directory,
        // delete em
        if (file.path().extension() == ".spv") {
            remove(file.path().string().c_str());
            continue;
        }

        Async::sDispatch([=]() {
            auto outfile = file.path().parent_path() / "bin" / file.path().filename();
            outfile.replace_extension(outfile.extension().string() + ".spv");

            if (!fs::exists(outfile) || fs::last_write_time(outfile) < file.last_write_time()) {
                auto success = glShader::glslangValidator(vulkanSDK, file, outfile);

                {
                    auto lock = Async::sLock();

                    if (!success) {
                        std::cout << "Compilation " << COUT_RED("failed") << " for shader: " << file.path().string() << '\n';
                    } else {
                        std::cout << "Compilation " << COUT_GREEN("finished") << " for shader: " << file.path().string() << '\n';
                    }
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
    ImGui_ImplSDL2_InitForOpenGL(window, &context);
    ImGui_ImplOpenGL3_Init("#version 450");
    ImGui_ImplOpenGL3_CreateDeviceObjects();

    // get GUI i/o and set a bunch of settings
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    // io.ConfigDockingWithShift = true;


    // initialise all the render passes
    icons =         std::make_unique<Icons>(viewport);
    bloom =         std::make_unique<Bloom>(viewport);
    gbuffer =       std::make_unique<GBuffer>(viewport);
    tonemap =       std::make_unique<Tonemap>(viewport);
    skinning =      std::make_unique<Skinning>();
    voxelize =      std::make_unique<Voxelize>(512);
    taaResolve =    std::make_unique<TAAResolve>(viewport);
    debugLines =    std::make_unique<DebugLines>();
    atmosphere =    std::make_unique<Atmosphere>(viewport);
    shadowMaps =    std::make_unique<ShadowMap>(viewport);
    debugvoxels =   std::make_unique<VoxelizeDebug>(viewport);
    deferShading =  std::make_unique<DeferredShading>(viewport);

    auto setDefaultTextureParams = [](GLuint& texture) {
        glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTextureParameteri(texture, GL_TEXTURE_WRAP_R, GL_REPEAT);
    };

    glCreateTextures(GL_TEXTURE_2D, 1, &blackTexture);
    glTextureStorage2D(blackTexture, 1, GL_RGBA8, 1, 1);
    setDefaultTextureParams(blackTexture);

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
    SDL_GL_DeleteContext(context);
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
}



void GLRenderer::render(const Scene& scene, const Viewport& viewport) {
    if (!timings.empty() && settings.disableTiming) {
        timings.clear();
    }

    // skin all meshes in the scene
    time("Skinning", [&]() {
        scene.view<const Skeleton, const Mesh>()
        .each([&](auto& animation, auto& mesh) {
            skinning->compute(mesh, animation);
        });
    });

    // render 4 * 4096 cascaded shadow maps
    time("Shadow cascades", [&]() {
        shadowMaps->render(viewport, scene);
    });

    // voxelize the Scene to a 3D texture
    if (settings.shouldVoxelize) {
        time("Voxelize", [&]() {
            voxelize->render(scene, viewport, *shadowMaps);
        });
    }

    // generate a geometry buffer with depth, normals, material and albedo
    time("GBuffer", [&]() {
        gbuffer->render(scene, viewport, frameNr);
    });

    // render the sky using ray marching for atmospheric scattering
    time("Atmosphere", [&]() {
        atmosphere->computeCubemaps(viewport, scene);
    });

    // fullscreen PBR deferred shading pass
    time("Deferred Shading", [&]() {
        deferShading->render(scene, viewport, *shadowMaps, *gbuffer, *atmosphere, *voxelize);
    });

    GLuint shadingResult = deferShading->result;

    if (settings.enableTAA) {
        time("TAA Resolve", [&]() {
            shadingResult = taaResolve->render(viewport, *gbuffer, *deferShading, frameNr);
        });
    }
    else {
        // if the cvar is enabled through cvars it doesnt reset the frameNr,
        // so we just do it here
        // frameNr = 0;
    }

    // render editor icons
    time("Icons", [&]() {
        icons->render(scene, viewport, shadingResult, gbuffer->entityTexture);
    });

    // generate downsampled bloom and do ACES tonemapping
    GLuint bloomTexture = blackTexture;

    if (settings.doBloom) {
        time("Bloom", [&]() {
            bloom->render(viewport, deferShading->bloomHighlights);
        });

        bloomTexture = bloom->bloomTexture;
    }

    time("Tonemap", [&]() {
        tonemap->render(shadingResult, bloomTexture);
    });

    if (settings.debugCascades) {
        time("Debug cascade", [&]() {
            shadowMaps->renderCascade(viewport, tonemap->framebuffer);
        });
    }

    // render debug lines / shapes
    time("Debug lines", [&]() {
        debugLines->render(viewport, tonemap->result, gbuffer->depthTexture);
    });

    // render 3D voxel texture size ^ 3 cubes
    if (settings.debugVoxels) {
        time("Debug voxels", [&]() {
            debugvoxels->render(viewport, tonemap->result, *voxelize);
        });
    }

    // build the imgui font texture
    if (!ImGui::GetIO().Fonts->TexID) {
        ImGui_ImplOpenGL3_CreateFontsTexture();
    }

    // bind and clear the window's swapchain
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // render ImGui
    time("ImGui", [&]() {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    });

    // increment frame counter
    frameNr = frameNr + 1;
}



void GLRenderer::addDebugLine(glm::vec3 p1, glm::vec3 p2) {
    debugLines->points.push_back(p1);
    debugLines->points.push_back(p2);
}



void GLRenderer::addDebugBox(glm::vec3 min, glm::vec3 max, glm::mat4& m) {
    addDebugLine(glm::vec3(m * glm::vec4(min.x, min.y, min.z, 1.0)), glm::vec3(m * glm::vec4(max.x, min.y, min.z, 1.0f)));
    addDebugLine(glm::vec3(m * glm::vec4(max.x, min.y, min.z, 1.0)), glm::vec3(m * glm::vec4(max.x, max.y, min.z, 1.0f)));
    addDebugLine(glm::vec3(m * glm::vec4(max.x, max.y, min.z, 1.0)), glm::vec3(m * glm::vec4(min.x, max.y, min.z, 1.0f)));
    addDebugLine(glm::vec3(m * glm::vec4(min.x, max.y, min.z, 1.0)), glm::vec3(m * glm::vec4(min.x, min.y, min.z, 1.0f)));
    addDebugLine(glm::vec3(m * glm::vec4(min.x, min.y, min.z, 1.0)), glm::vec3(m * glm::vec4(min.x, min.y, max.z, 1.0f)));
    addDebugLine(glm::vec3(m * glm::vec4(max.x, min.y, min.z, 1.0)), glm::vec3(m * glm::vec4(max.x, min.y, max.z, 1.0f)));
    addDebugLine(glm::vec3(m * glm::vec4(max.x, max.y, min.z, 1.0)), glm::vec3(m * glm::vec4(max.x, max.y, max.z, 1.0f)));
    addDebugLine(glm::vec3(m * glm::vec4(min.x, max.y, min.z, 1.0)), glm::vec3(m * glm::vec4(min.x, max.y, max.z, 1.0f)));
    addDebugLine(glm::vec3(m * glm::vec4(min.x, min.y, max.z, 1.0)), glm::vec3(m * glm::vec4(max.x, min.y, max.z, 1.0f)));
    addDebugLine(glm::vec3(m * glm::vec4(max.x, min.y, max.z, 1.0)), glm::vec3(m * glm::vec4(max.x, max.y, max.z, 1.0f)));
    addDebugLine(glm::vec3(m * glm::vec4(max.x, max.y, max.z, 1.0)), glm::vec3(m * glm::vec4(min.x, max.y, max.z, 1.0f)));
    addDebugLine(glm::vec3(m * glm::vec4(min.x, max.y, max.z, 1.0)), glm::vec3(m * glm::vec4(min.x, min.y, max.z, 1.0f)));
}



template<typename Lambda>
void GLRenderer::time(const std::string& name, Lambda&& lambda) {
    if (!settings.disableTiming) {
        timings.insert({ name, std::make_unique<GLTimer>() });

        timings[name]->begin();

        lambda();

        timings[name]->end();
    }
    else {
        lambda();
    }
}



void GLRenderer::uploadMeshBuffers(Mesh& mesh) {
    auto vertices = mesh.GetInterleavedVertices();

    glCreateBuffers(1, &mesh.vertexBuffer);
    glNamedBufferData(mesh.vertexBuffer, sizeof(float) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

    glCreateBuffers(1, &mesh.indexBuffer);
    glNamedBufferData(mesh.indexBuffer, sizeof(uint32_t) * mesh.indices.size(), mesh.indices.data(), GL_STATIC_DRAW);
}



void GLRenderer::destroyMeshBuffers(Mesh& mesh) {
    glDeleteBuffers(1, &mesh.vertexBuffer);
    glDeleteBuffers(1, &mesh.indexBuffer);
    mesh.vertexBuffer = 0, mesh.indexBuffer = 0;
}

void GLRenderer::UploadSkeletonBuffers(Skeleton& skeleton, Mesh& mesh) {
    glCreateBuffers(1, &skeleton.boneIndexBuffer);
    glNamedBufferData(skeleton.boneIndexBuffer, skeleton.m_BoneIndices.size() * sizeof(glm::ivec4), skeleton.m_BoneIndices.data(), GL_STATIC_COPY);

    glCreateBuffers(1, &skeleton.boneWeightBuffer);
    glNamedBufferData(skeleton.boneWeightBuffer, skeleton.m_BoneWeights.size() * sizeof(glm::vec4), skeleton.m_BoneWeights.data(), GL_STATIC_COPY);

    glCreateBuffers(1, &skeleton.boneTransformsBuffer);
    glNamedBufferData(skeleton.boneTransformsBuffer, skeleton.m_BoneTransforms.size() * sizeof(glm::mat4), skeleton.m_BoneTransforms.data(), GL_DYNAMIC_READ);

    auto originalMeshBuffer = mesh.GetInterleavedVertices();

    if (skeleton.skinnedVertexBuffer) glDeleteBuffers(1, &skeleton.skinnedVertexBuffer);
    glCreateBuffers(1, &skeleton.skinnedVertexBuffer);
    glNamedBufferData(skeleton.skinnedVertexBuffer, sizeof(originalMeshBuffer[0]) * originalMeshBuffer.size(), originalMeshBuffer.data(), GL_STATIC_DRAW);
}


void GLRenderer::DestroySkeletonBuffers(Skeleton& skeleton) {
    glDeleteBuffers(1, &skeleton.boneIndexBuffer);
    glDeleteBuffers(1, &skeleton.boneWeightBuffer);
    glDeleteBuffers(1, &skeleton.skinnedVertexBuffer);
    glDeleteBuffers(1, &skeleton.boneTransformsBuffer);
}



void GLRenderer::destroyMaterialTextures(Material& material, Assets& assets) {
    glDeleteTextures(1, &material.gpuAlbedoMap);
    glDeleteTextures(1, &material.gpuNormalMap);
    glDeleteTextures(1, &material.gpuMetallicRoughnessMap);
    material.gpuAlbedoMap = 0, material.gpuNormalMap = 0, material.gpuMetallicRoughnessMap = 0;

    assets.Release(material.albedoFile);
    assets.Release(material.normalFile);
    assets.Release(material.metalroughFile);
}



void GLRenderer::uploadMaterialTextures(Material& material, Assets& assets) {
    if (auto asset = assets.Get<TextureAsset>(material.albedoFile); asset) {
        material.gpuAlbedoMap = GLRenderer::uploadTextureFromAsset(asset, true);
    }

    if (auto asset = assets.Get<TextureAsset>(material.normalFile); asset) {
        material.gpuNormalMap = GLRenderer::uploadTextureFromAsset(asset);
    }

    if (auto asset = assets.Get<TextureAsset>(material.metalroughFile); asset) {
        material.gpuMetallicRoughnessMap = GLRenderer::uploadTextureFromAsset(asset);
    }
}



GLuint GLRenderer::uploadTextureFromAsset(const TextureAsset::Ptr& asset, bool sRGB) {
    assert(asset);

    GLuint texture = 0;
    auto headerPtr = asset->GetHeader();
    auto dataPtr = asset->GetData();

    auto format = sRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;

    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    glTextureStorage2D(texture, headerPtr->dwMipMapCount, format, headerPtr->dwWidth, headerPtr->dwHeight);

    for (unsigned int mip = 0; mip < headerPtr->dwMipMapCount; mip++) {
        glm::ivec2 dimensions = { std::max(headerPtr->dwWidth >> mip, 1ul), std::max(headerPtr->dwHeight >> mip, 1ul) };
        size_t dataSize = std::max(1, ((dimensions.x + 3) / 4)) * std::max(1, ((dimensions.y + 3) / 4)) * 16;
        glCompressedTextureSubImage2D(texture, mip, 0, 0, dimensions.x, dimensions.y, format, (GLsizei)dataSize, dataPtr);
        dataPtr += dimensions.x * dimensions.y;
    }

    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return texture;
}



void GLRenderer::createRenderTargets(const Viewport& viewport) {
    frameNr = 0;

    deferShading->destroyRenderTargets();
    deferShading->createRenderTargets(viewport);

    debugvoxels->destroyRenderTargets();
    debugvoxels->createRenderTargets(viewport);

    tonemap->destroyRenderTargets();
    tonemap->createRenderTargets(viewport);

    gbuffer->destroyRenderTargets();
    gbuffer->createRenderTargets(viewport);

    bloom->destroyRenderTargets();
    bloom->createRenderTargets(viewport);

    icons->destroyRenderTargets();
    icons->createRenderTargets(viewport);

    atmosphere->destroyRenderTargets();
    atmosphere->createRenderTargets(viewport);

    taaResolve->destroyRenderTargets();
    taaResolve->createRenderTargets(viewport);

    shadowMaps->updatePerspectiveConstants(viewport);
}

} // namespace Raekor