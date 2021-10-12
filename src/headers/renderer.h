#pragma once

#include "util.h"
#include "camera.h"
#include "renderpass.h"

namespace Raekor {

class Scene;
class Async;

class GLRenderer {
    friend class RandomWidget;
    friend class ViewportWidget;

    struct {
        int& vsync = ConVars::create("r_vsync", 1);
        int& doBloom = ConVars::create("r_bloom", 0);
        int& debugVoxels = ConVars::create("r_voxelize_debug", 0);
        int& shouldVoxelize = ConVars::create("r_voxelize", 1);
        int& debugCascades = ConVars::create("r_debugCascades", 0);
    } settings;

public:
    GLRenderer(SDL_Window* window, Viewport& viewport);
    ~GLRenderer();

    void ImGui_Render();
    void ImGui_NewFrame(SDL_Window* window);

    void drawLine(glm::vec3 p1, glm::vec3 p2);
    void drawBox(glm::vec3 min, glm::vec3 max, glm::mat4& m = glm::mat4(1.0f));

    void render(const Scene& scene, const Viewport& viewport);
    void createRenderTargets(const Viewport& viewport);

public:
    std::unique_ptr<Bloom> bloom;
    std::unique_ptr<GBuffer> gbuffer;
    std::unique_ptr<Icons> icons;
    std::unique_ptr<DebugLines> lines;
    std::unique_ptr<Voxelize> voxelize;
    std::unique_ptr<Tonemap> tonemap;
    std::unique_ptr<ShadowMap> shadows;
    std::unique_ptr<Skinning> skinning;
    std::unique_ptr<Atmosphere> sky;
    std::unique_ptr<DeferredShading> shading;
    std::unique_ptr<VoxelizeDebug> debugvoxels;

private:
    GLuint blackTexture;
    SDL_GLContext context;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

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
    inline static RenderAPI activeAPI = RenderAPI::OPENGL;
    inline static Renderer* instance = nullptr;
};

} // namespace Raekor