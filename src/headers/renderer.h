#pragma once

#include "util.h"
#include "camera.h"
#include "renderpass.h"

namespace Raekor {

class GLRenderer {
    struct {
        int& doBloom = ConVars::create("r_bloom", 0);
        int& debugVoxels = ConVars::create("r_voxelize_debug", 0);
        int& shouldVoxelize = ConVars::create("r_voxelize", 1);
    } settings;

public:
    GLRenderer(SDL_Window* window, Viewport& viewport);
    ~GLRenderer();

    void ImGui_Render();
    void ImGui_NewFrame(SDL_Window* window);

    void drawLine(glm::vec3 p1, glm::vec3 p2);
    void drawBox(glm::vec3 min, glm::vec3 max, glm::mat4& m = glm::mat4(1.0f));

    void render(entt::registry& scene, Viewport& viewport);
    void createResources(Viewport& viewport);

public:
    std::unique_ptr<Bloom> bloomPass;
    std::unique_ptr<GBuffer> GBufferPass;
    std::unique_ptr<Icons> worldIconsPass;
    std::unique_ptr<DebugLines> debugPass;
    std::unique_ptr<Voxelize> voxelizePass;
    std::unique_ptr<Tonemap> tonemappingPass;
    std::unique_ptr<ShadowMap> shadowMapPass;
    std::unique_ptr<SkinCompute> skinningPass;
    std::unique_ptr<Atmosphere> atmospherePass;
    std::unique_ptr<DeferredShading> deferredPass;
    std::unique_ptr<VoxelizeDebug> voxelizeDebugPass;

public:
    bool vsync = true;

private:
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