#pragma once

#include "Raekor/util.h"
#include "Raekor/camera.h"
#include "renderpass.h"

namespace Raekor {

class Scene;
class Async;

class GLRenderer {
    friend class RandomWidget;
    friend class ViewportWidget;

    struct {
        int& vsync              = CVars::sCreate("r_vsync", 1);
        int& doBloom            = CVars::sCreate("r_bloom", 0);
        int& paused             = CVars::sCreate("r_paused", 0);
        int& enableTAA          = CVars::sCreate("r_taa", 1);
        int& debugVoxels        = CVars::sCreate("r_voxelize_debug", 0);
        int& debugCascades      = CVars::sCreate("r_debug_cascades", 0);
        int& disableTiming      = CVars::sCreate("r_disable_timings", 0);
        int& shouldVoxelize     = CVars::sCreate("r_voxelize", 1);
    } settings;

public:
    GLRenderer(SDL_Window* ioWindow, Viewport& ioViewport);
    ~GLRenderer();

    void Render(const Scene& inScene, const Viewport& inViewport);
    void CreateRenderTargets(const Viewport& inViewport);

    void AddDebugLine(glm::vec3 inP1, glm::vec3 inP2);
    void AddDebugBox(glm::vec3 inMin, glm::vec3 inMax, glm::mat4& inTransform = glm::mat4(1.0f));

    template<typename Lambda>
    void TimeOpenGL(const std::string& inName, Lambda&& inLambda);

    static void sUploadMeshBuffers(Mesh& inMesh);
    static void sDestroyMeshBuffers(Mesh& inMesh);

    static void sUploadSkeletonBuffers(Skeleton& inSkeleton, Mesh& inMesh);
    static void sDestroySkeletonBuffers(Skeleton& inSkeleton);

    static void sUploadMaterialTextures(Material& inMaterial, Assets& inAssets);
    static void sDestroyMaterialTextures(Material& inMaterial, Assets& inAssets);
    static GLuint sUploadTextureFromAsset(const TextureAsset::Ptr& inAsset, bool inIsSRGB = false);

public:
    std::shared_ptr<Bloom>                      m_Bloom;
    std::shared_ptr<Icons>                      m_Icons;
    std::shared_ptr<Tonemap>                    m_Tonemap;
    std::shared_ptr<GBuffer>                    m_GBuffer;
    std::shared_ptr<Voxelize>                   m_Voxelize;
    std::shared_ptr<Skinning>                   m_Skinning;
    std::shared_ptr<ShadowMap>                  m_ShadowMaps;
    std::shared_ptr<Atmosphere>                 m_Atmosphere;
    std::shared_ptr<TAAResolve>                 m_ResolveTAA;
    std::shared_ptr<DebugLines>                 m_DebugLines;
    std::shared_ptr<VoxelizeDebug>              m_DebugVoxels;
    std::shared_ptr<DeferredShading>            m_DeferredShading;
    std::vector<std::shared_ptr<RenderPass>>    m_RenderPasses;

    std::map<std::string, std::unique_ptr<GLTimer>> m_Timings;

private:
    uint32_t m_FrameNr = 0;
    GLuint m_DefaultBlackTexture = 0;
    SDL_GLContext m_GLContext = nullptr;
};

} // namespace Raekor