#include "pch.h"
#include "renderer.h"

#ifdef _WIN32
    #include "DXRenderer.h"
#endif

namespace Raekor {

static void log_msg(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
    if (severity != GL_DEBUG_SEVERITY_NOTIFICATION) {
        std::cout << message << std::endl;
    }
}

// global enum for changing the active render API
RenderAPI Renderer::activeAPI = RenderAPI::OPENGL;

Renderer* Renderer::construct(SDL_Window* window) {
    switch (activeAPI) {
        case RenderAPI::OPENGL: {
            return new GLRenderer(window);
        } break;
#ifdef _WIN32
        case RenderAPI::DIRECTX11: {
            return new DXRenderer(window);
        } break;
#endif
    }
    return nullptr;
}


GLRenderer::GLRenderer(SDL_Window* window) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    
    context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);
    m_assert(gl3wInit() == 0, "failed to init gl3w");
    
    // print the initialized openGL specs
    std::cout << "GL INFO: OpenGL " << glGetString(GL_VERSION);
    printf("GL INFO: OpenGL %s, GLSL %s\n", glGetString(GL_VERSION),
        glGetString(GL_SHADING_LANGUAGE_VERSION));
    
    ImGui_ImplSDL2_InitForOpenGL(window, &context);
    ImGui_ImplOpenGL3_Init("#version 330");

    glDebugMessageCallback(log_msg, nullptr);

    // set opengl depth testing and culling
    //glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glFrontFace(GL_CCW);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // one time setup for binding vertey arrays
    unsigned int vertex_array_id;
    glGenVertexArrays(1, &vertex_array_id);
    glBindVertexArray(vertex_array_id);
}

GLRenderer::~GLRenderer() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    SDL_GL_DeleteContext(context);
}

void GLRenderer::Clear(glm::vec4 color) {
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLRenderer::ImGui_NewFrame(SDL_Window* window) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();
}

void GLRenderer::ImGui_Render() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GLRenderer::DrawIndexed(unsigned int size) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDrawElements(GL_TRIANGLES, size, GL_UNSIGNED_INT, nullptr);
    glDisable(GL_DEPTH_TEST);
}



} // namespace Raekor