#pragma once

#include "pch.h"
#include "util.h"

namespace Raekor {

enum class RenderAPI {
    OPENGL, DIRECTX11, VULKAN
};

class Renderer {
public:
    virtual ~Renderer() {}
    // static constructor that abstracts the runtime renderer construction
    static Renderer* construct(SDL_Window* window);

    // functions for getting and setting the active render API
    inline static RenderAPI get_activeAPI() { return activeAPI; }
    static bool set_activeAPI(const RenderAPI new_active);

    // ImGui specific render API methods
    virtual void ImGui_Render()                                             = 0;
    virtual void ImGui_NewFrame(SDL_Window* window)                         = 0;

    // Render API methods, these should be used in combination with Raekor's vertex and index buffers
    virtual void Clear(glm::vec4 color)                                     = 0;
    virtual void DrawIndexed(unsigned int size, bool depth_test = true)     = 0;
    virtual void SwapBuffers(bool vsync) const                              = 0;

protected:
    SDL_Window* render_window;
    static RenderAPI activeAPI;
};

class Render {
public:
    inline static void Init(SDL_Window* window) { renderer.reset(Renderer::construct(window)); }
    inline static void Reset(SDL_Window* window) { renderer.reset(Renderer::construct(window)); }
    inline static void Clear(glm::vec4 color) { renderer->Clear(color); }

    inline static void ImGui_Render() { renderer->ImGui_Render(); }
    inline static void ImGui_NewFrame(SDL_Window* window) { renderer->ImGui_NewFrame(window); }

    inline static void DrawIndexed(unsigned int size, bool depth_test = true) { renderer->DrawIndexed(size, depth_test); }
    inline static void SwapBuffers(bool vsync) { renderer->SwapBuffers(vsync); }

private:
    static std::unique_ptr<Renderer> renderer;
};

class GLRenderer : public Renderer {
public:
    GLRenderer(SDL_Window* window);
    ~GLRenderer();

    virtual void ImGui_Render()                                             override;
    virtual void ImGui_NewFrame(SDL_Window* window)                         override;

    virtual void Clear(glm::vec4 color)                                     override;
    virtual void DrawIndexed(unsigned int size, bool depth_test = true)     override;
    virtual void SwapBuffers(bool vsync) const                              override;
private:
    SDL_GLContext context;

};

} // namespace Raekor