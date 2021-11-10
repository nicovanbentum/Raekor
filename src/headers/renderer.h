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
        int& enableTAA = ConVars::create("r_taa", 1);
        int& shouldVoxelize = ConVars::create("r_voxelize", 1);
        int& debugVoxels = ConVars::create("r_voxelize_debug", 0);
        int& debugCascades = ConVars::create("r_debug_cascades", 0);
    } settings;

public:
    GLRenderer(SDL_Window* window, Viewport& viewport);
    ~GLRenderer();

    void render(const Scene& scene, const Viewport& viewport);
    void createRenderTargets(const Viewport& viewport);

    void addDebugLine(glm::vec3 p1, glm::vec3 p2);
    void addDebugBox(glm::vec3 min, glm::vec3 max, glm::mat4& m = glm::mat4(1.0f));

    static void uploadMeshBuffers(Mesh& mesh);
    static void destroyMeshBuffers(Mesh& mesh);

    static void uploadMaterialTextures(Material& material, Assets& assets);
    static void destroyMaterialTextures(Material& material, Assets& assets);
    static GLuint uploadTextureFromAsset(const TextureAsset::Ptr& asset, bool sRGB = false);
    
public:
    std::unique_ptr<Bloom>              bloom;
    std::unique_ptr<Icons>              icons;
    std::unique_ptr<Tonemap>            tonemap;
    std::unique_ptr<GBuffer>            gbuffer;
    std::unique_ptr<Voxelize>           voxelize;
    std::unique_ptr<Skinning>           skinning;
    std::unique_ptr<ShadowMap>          shadowMaps;
    std::unique_ptr<Atmosphere>         atmosphere;
    std::unique_ptr<TAAResolve>         taaResolve;
    std::unique_ptr<DebugLines>         debugLines;
    std::unique_ptr<VoxelizeDebug>      debugvoxels;
    std::unique_ptr<DeferredShading>    deferShading;

private:
    uint32_t frameNr = 0;
    GLuint blackTexture;
    SDL_GLContext context;
};

} // namespace Raekor