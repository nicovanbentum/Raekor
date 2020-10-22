#pragma once

#include "util.h"

namespace Raekor {

enum class RenderAPI {
    OPENGL, DIRECTX11, VULKAN
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class Renderer {
public:
    // Render API methods
    static void Init(SDL_Window* window);
    static void Clear(glm::vec4 color);
    static void ImGuiRender();
    static void ImGuiNewFrame(SDL_Window* window);
    static void DrawIndexed(unsigned int size);
    static void SwapBuffers(bool vsync);

    // API methods to get and set the API used
    static RenderAPI getActiveAPI();
    static void setAPI(const RenderAPI api);
    
    virtual ~Renderer() {}

private:
    // interface functions 
    virtual void impl_ImGui_Render()                                             = 0;
    virtual void impl_ImGui_NewFrame(SDL_Window* window)                         = 0;
    virtual void impl_Clear(glm::vec4 color)                                     = 0;
    virtual void impl_DrawIndexed(unsigned int size)                             = 0;
    virtual void impl_SwapBuffers(bool vsync) const                              = 0;

protected:
    SDL_Window* renderWindow;

private:
    static RenderAPI activeAPI;
    static Renderer* instance;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class GLRenderer {
public:
    GLRenderer(SDL_Window* window);
    ~GLRenderer();

    void ImGui_Render();
    void ImGui_NewFrame(SDL_Window* window);
    void Clear(glm::vec4 color);
    void DrawIndexed(unsigned int size);
    void SwapBuffers(SDL_Window* window, bool vsync);

private:

    SDL_GLContext context;

};

} // namespace Raekor