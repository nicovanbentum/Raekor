#include "pch.h"
#include "renderer.h"

#ifdef _WIN32
#include "platform/windows/DXRenderer.h"
#endif

#include "camera.h"
#include "renderpass.h"

namespace Raekor
{

void MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
    if (severity != GL_DEBUG_SEVERITY_NOTIFICATION) {
        fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
            (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
            type, severity, message);

        switch (id) {
            case 131218: return; // shader state recompilation
#ifndef NDEBUG
            default:
                assert(false);
#endif
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

GLRenderer::GLRenderer(SDL_Window* window, Viewport& viewport) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);

    // Load GL extensions using glad
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize the OpenGL context.\n";
        return;
    }

    // Loaded OpenGL successfully.
    std::cout << "OpenGL version loaded: " << GLVersion.major << "."
        << GLVersion.minor << std::endl;


    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, &context);
    ImGui_ImplOpenGL3_Init("#version 450");

    // get GUI i/o and set a bunch of settings
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigDockingWithShift = true;

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

    // one time setup for binding vertex arrays
    unsigned int vertexArrayID;
    glGenVertexArrays(1, &vertexArrayID);
    glBindVertexArray(vertexArrayID);

    // initialize default gpu resources
    ecs::MaterialComponent::Default = ecs::MaterialComponent
    {
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.0f, 1.0f
    };

    ecs::MaterialComponent::Default.createAlbedoTexture();
    ecs::MaterialComponent::Default.createMetalRoughTexture();
    ecs::MaterialComponent::Default.createNormalTexture();

    skinningPass = std::make_unique<SkinCompute>();
    voxelizePass = std::make_unique<Voxelize>(256);
    shadowMapPass = std::make_unique<ShadowMap>(4096, 4096);
    tonemappingPass = std::make_unique<Tonemap>(viewport);
    GBufferPass = std::make_unique<GBuffer>(viewport);
    deferredPass = std::make_unique<DeferredShading>(viewport);
    debugPass = std::make_unique<DebugLines>();
    voxelizeDebugPass = std::make_unique<VoxelizeDebug>(viewport);
    bloomPass = std::make_unique<Bloom>(viewport);
    worldIconsPass = std::make_unique<Icons>(viewport);
    atmospherePass = std::make_unique<Atmosphere>(viewport);
}

GLRenderer::~GLRenderer() {
    ImGui_ImplOpenGL3_DestroyDeviceObjects();
    SDL_GL_DeleteContext(context);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void GLRenderer::ImGui_NewFrame(SDL_Window* window) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void GLRenderer::render(entt::registry& scene, Viewport& viewport) {
    scene.view<ecs::MeshAnimationComponent, ecs::MeshComponent>().each([&](auto& animation, auto& mesh) {
        skinningPass->render(mesh, animation);
    });

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // generate sun shadow map
    glViewport(0, 0, 4096, 4096);
    shadowMapPass->render(viewport, scene);

    if (settings.shouldVoxelize) {
        voxelizePass->render(scene, viewport, shadowMapPass.get());
    }

    glViewport(0, 0, viewport.size.x, viewport.size.y);

    GBufferPass->render(scene, viewport);

    deferredPass->render(scene, viewport, shadowMapPass.get(), GBufferPass.get(), voxelizePass.get());

    atmospherePass->render(viewport, scene, deferredPass->result, GBufferPass->depthTexture);
    
    worldIconsPass->render(scene, viewport, deferredPass->result, GBufferPass->entityTexture);

    if (settings.doBloom) {
        bloomPass->render(viewport, deferredPass->bloomHighlights);
        tonemappingPass->render(deferredPass->result, bloomPass->bloomTexture);
    } else {
        static unsigned int blackTexture = 0;
        if (blackTexture == 0) {
            glCreateTextures(GL_TEXTURE_2D, 1, &blackTexture);
            glTextureStorage2D(blackTexture, 1, GL_RGBA8, 1, 1);
            glTextureParameteri(blackTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTextureParameteri(blackTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTextureParameteri(blackTexture, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTextureParameteri(blackTexture, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTextureParameteri(blackTexture, GL_TEXTURE_WRAP_R, GL_REPEAT);
        }

        tonemappingPass->render(deferredPass->result, blackTexture);
    }
    
    debugPass->render(scene, viewport, tonemappingPass->result, GBufferPass->depthTexture);

    if (settings.debugVoxels) {
        voxelizeDebugPass->render(viewport, tonemappingPass->result, voxelizePass.get());
    }
}

void GLRenderer::drawLine(glm::vec3 p1, glm::vec3 p2) {
    debugPass->points.push_back(p1);
    debugPass->points.push_back(p2);
}

void GLRenderer::drawBox(glm::vec3 min, glm::vec3 max, glm::mat4& m) {
    drawLine(glm::vec3(m * glm::vec4(min.x, min.y, min.z, 1.0)), glm::vec3(m * glm::vec4(max.x, min.y, min.z, 1.0f)));
    drawLine(glm::vec3(m * glm::vec4(max.x, min.y, min.z, 1.0)), glm::vec3(m * glm::vec4(max.x, max.y, min.z, 1.0f)));
    drawLine(glm::vec3(m * glm::vec4(max.x, max.y, min.z, 1.0)), glm::vec3(m * glm::vec4(min.x, max.y, min.z, 1.0f)));
    drawLine(glm::vec3(m * glm::vec4(min.x, max.y, min.z, 1.0)), glm::vec3(m * glm::vec4(min.x, min.y, min.z, 1.0f)));
    drawLine(glm::vec3(m * glm::vec4(min.x, min.y, min.z, 1.0)), glm::vec3(m * glm::vec4(min.x, min.y, max.z, 1.0f)));
    drawLine(glm::vec3(m * glm::vec4(max.x, min.y, min.z, 1.0)), glm::vec3(m * glm::vec4(max.x, min.y, max.z, 1.0f)));
    drawLine(glm::vec3(m * glm::vec4(max.x, max.y, min.z, 1.0)), glm::vec3(m * glm::vec4(max.x, max.y, max.z, 1.0f)));
    drawLine(glm::vec3(m * glm::vec4(min.x, max.y, min.z, 1.0)), glm::vec3(m * glm::vec4(min.x, max.y, max.z, 1.0f)));
    drawLine(glm::vec3(m * glm::vec4(min.x, min.y, max.z, 1.0)), glm::vec3(m * glm::vec4(max.x, min.y, max.z, 1.0f)));
    drawLine(glm::vec3(m * glm::vec4(max.x, min.y, max.z, 1.0)), glm::vec3(m * glm::vec4(max.x, max.y, max.z, 1.0f)));
    drawLine(glm::vec3(m * glm::vec4(max.x, max.y, max.z, 1.0)), glm::vec3(m * glm::vec4(min.x, max.y, max.z, 1.0f)));
    drawLine(glm::vec3(m * glm::vec4(min.x, max.y, max.z, 1.0)), glm::vec3(m * glm::vec4(min.x, min.y, max.z, 1.0f)));
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void GLRenderer::createResources(Viewport& viewport) {
    deferredPass->deleteResources();
    deferredPass->createResources(viewport);

    voxelizeDebugPass->deleteResources();
    voxelizeDebugPass->createResources(viewport);

    tonemappingPass->deleteResources();
    tonemappingPass->createResources(viewport);

    GBufferPass->deleteResources();
    GBufferPass->createResources(viewport);

    bloomPass->deleteResources();
    bloomPass->createResources(viewport);

    worldIconsPass->destroyResources();
    worldIconsPass->createResources(viewport);

    atmospherePass->destroyResources();
    atmospherePass->createResources(viewport);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void GLRenderer::ImGui_Render() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::Init(SDL_Window* window) {
    switch (activeAPI) {
        case RenderAPI::OPENGL:
        {
            instance = nullptr;
        } break;
#ifdef _WIN32
        case RenderAPI::DIRECTX11:
        {
            instance = new DXRenderer(window);
        } break;
#endif
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::Clear(glm::vec4 color) {
    instance->impl_Clear(color);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::ImGuiRender() {
    instance->impl_ImGui_Render();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::ImGuiNewFrame(SDL_Window* window) {
    instance->impl_ImGui_NewFrame(window);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::DrawIndexed(unsigned int size) {
    instance->impl_DrawIndexed(size);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::SwapBuffers(bool vsync) {
    instance->impl_SwapBuffers(vsync);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

RenderAPI Renderer::getActiveAPI() {
    return activeAPI;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::setAPI(const RenderAPI api) {
    activeAPI = api;
}

} // namespace Raekor