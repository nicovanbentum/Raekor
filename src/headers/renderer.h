#pragma once

#include "pch.h"
#include "util.h"
#include "mesh.h"
#include "model.h"


namespace Raekor {

enum class RenderAPI {
    OPENGL, DIRECTX11
};

class Renderer {
public:
    // static constructor that abstracts the runtime renderer construction
    static Renderer* construct(SDL_Window* window);

    // functions for getting and setting the active render API
    inline static RenderAPI get_activeAPI() { return activeAPI; }
    inline static void set_activeAPI(const RenderAPI new_active) { activeAPI = new_active; }

    // ImGui specific render API methods
    virtual void ImGui_Render()                                 = 0;
    virtual void ImGui_NewFrame(SDL_Window* window)             = 0;

    // Render API methods, these should be used in combination with Raekor's vertex and index buffers
    virtual void Clear(glm::vec4 color)                         = 0;
    virtual void DrawIndexed(unsigned int size)                 = 0;

private:
    static RenderAPI activeAPI;
};

class GLRenderer : public Renderer {
public:
    GLRenderer(SDL_Window* window);
    ~GLRenderer();

    virtual void ImGui_Render()                             override;
    virtual void ImGui_NewFrame(SDL_Window* window)         override;

    virtual void Clear(glm::vec4 color)                     override;
    virtual void DrawIndexed(unsigned int size)             override;
private:
    SDL_GLContext context;
};

} // namespace Raekor