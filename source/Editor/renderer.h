#pragma once

#include "Raekor/util.h"
#include "Raekor/camera.h"
#include "Raekor/application.h"
#include "renderpass.h"

namespace Raekor {

class Scene;
class Async;

class GLRenderer : public IRenderer {
public:
    friend class RandomWidget;
    friend class ViewportWidget;

    using GPUTimings = std::map<std::string, std::unique_ptr<GLTimer>>;

public:
    GLRenderer(SDL_Window* inWindow, Viewport& ioViewport);
    ~GLRenderer();

    void Render(const Scene& inScene, const Viewport& inViewport);
    void CreateRenderTargets(const Viewport& inViewport);

    void AddDebugLine(glm::vec3 inP1, glm::vec3 inP2);
    void AddDebugBox(glm::vec3 inMin, glm::vec3 inMax, const glm::mat4& inTransform = glm::mat4(1.0f));

    template<typename Lambda>
    void TimeOpenGL(const std::string& inName, Lambda&& inLambda);

    uint64_t GetDisplayTexture() { return m_Tonemap ? m_GBuffer->normalTexture : entt::null; }
    uint64_t GetImGuiTextureID(uint32_t inTextureID) { return inTextureID; }

    uint32_t GetScreenshotBuffer(uint8_t* ioBuffer);
    uint32_t GetSelectedEntity(uint32_t inScreenPosX, uint32_t inScreenPosY) { return m_GBuffer ? m_GBuffer->readEntity(inScreenPosX, inScreenPosY) : entt::null; }
    
    void OnResize(const Viewport& inViewport) { 
        CreateRenderTargets(inViewport);
    }

    void DrawImGui(Scene& inScene, const Viewport& inViewport);

    GPUTimings& GetGPUTimings() { return m_Timings; }

    void UploadMeshBuffers(Mesh& inMesh) override;
    void DestroyMeshBuffers(Mesh& inMesh);

    void UploadSkeletonBuffers(Skeleton& inSkeleton, Mesh& inMesh) override;
    void DestroySkeletonBuffers(Skeleton& inSkeleton);

    void DestroyMaterialTextures(Material& inMaterial, Assets& inAssets);

    uint32_t UploadTextureFromAsset(const TextureAsset::Ptr& inAsset, bool inIsSRGB = false) override;



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

    GPUTimings m_Timings;

private:
    uint32_t      m_FrameNr = 0;
    uint32_t      m_DefaultBlackTexture = 0;
    SDL_Window*   m_Window = nullptr;
    SDL_GLContext m_GLContext = nullptr;
};

} // namespace Raekor