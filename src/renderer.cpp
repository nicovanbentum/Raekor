#include "pch.h"
#include "renderer.h"

#ifdef _WIN32
    #include "platform/windows/DXRenderer.h"
#endif

#include "components.h"

namespace Raekor {

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

// globals for the active API and renderer
RenderAPI Renderer::activeAPI = RenderAPI::OPENGL;
Renderer* Renderer::instance = nullptr;

//////////////////////////////////////////////////////////////////////////////////////////////////

GLRenderer::GLRenderer(SDL_Window* window) {
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

    // one time setup for binding vertey arrays
    unsigned int vertexArrayID;
    glGenVertexArrays(1, &vertexArrayID);
    glBindVertexArray(vertexArrayID);

    // initialize default gpu resources
    ecs::MaterialComponent::Default = ecs::MaterialComponent {
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.0f, 1.0f
    };

    ecs::MaterialComponent::Default.createAlbedoTexture();
    ecs::MaterialComponent::Default.createMetalRoughTexture();
    ecs::MaterialComponent::Default.createNormalTexture();

}

GLRenderer::~GLRenderer() {
    ImGui_ImplOpenGL3_DestroyDeviceObjects();
    SDL_GL_DeleteContext(context);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void GLRenderer::Clear(glm::vec4 color) {
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void GLRenderer::ImGui_NewFrame(SDL_Window* window) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void GLRenderer::ImGui_Render() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void GLRenderer::SwapBuffers(SDL_Window* window, bool vsync) {
    SDL_GL_SetSwapInterval(vsync);
    SDL_GL_SwapWindow(window);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void GLRenderer::DrawIndexed(unsigned int size) {
    glDrawElements(GL_TRIANGLES, size, GL_UNSIGNED_INT, nullptr);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::Init(SDL_Window * window) {
    switch (activeAPI) {
        case RenderAPI::OPENGL: {
            instance = nullptr;
        } break;
#ifdef _WIN32
        case RenderAPI::DIRECTX11: {
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