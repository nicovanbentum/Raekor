#pragma once

#include "util.h"
#include "camera.h"
#include "renderpass.h"

namespace Raekor {

class GLRenderer {
public:
    GLRenderer(SDL_Window* window, Viewport& viewport);
    ~GLRenderer();

    void ImGui_Render();
    void ImGui_NewFrame(SDL_Window* window);

    void render(entt::registry& scene, Viewport& viewport, entt::entity& active);
    void createResources(Viewport& viewport);

public:
    std::unique_ptr<RenderPass::HDRSky>             skyPass;
    std::unique_ptr<RenderPass::Bloom>              bloomPass;
    std::unique_ptr<RenderPass::Skinning>           skinningPass;
    std::unique_ptr<RenderPass::ShadowMap>          shadowMapPass;
    std::unique_ptr<RenderPass::WorldIcons>         worldIconsPass;
    std::unique_ptr<RenderPass::Tonemapping>        tonemappingPass;
    std::unique_ptr<RenderPass::Voxelization>       voxelizationPass;
    std::unique_ptr<RenderPass::GeometryBuffer>     geometryBufferPass;
    std::unique_ptr<RenderPass::DeferredLighting>   DeferredLightingPass;
    std::unique_ptr<RenderPass::BoundingBoxDebug>   boundingBoxDebugPass;
    std::unique_ptr<RenderPass::VoxelizationDebug>  voxelizationDebugPass;

    bool doBloom = false;
    bool shouldVoxelize = true;
    bool debugVoxels = false;
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