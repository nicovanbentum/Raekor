#pragma once

#include "Raekor/util.h"
#include "Raekor/camera.h"
#include "Raekor/application.h"
#include "renderpass.h"

namespace Raekor {
    class Scene;
    class Async;
}

namespace Raekor::GL {


class Renderer : public IRenderInterface {
public:
    Renderer(SDL_Window* inWindow, Viewport& ioViewport);
    ~Renderer();

    void Render(const Scene& inScene, const Viewport& inViewport);
    void CreateRenderTargets(const Viewport& inViewport);

    void AddDebugLine(Vec3 inP1, Vec3 inP2);
    void AddDebugBox(Vec3 inMin, Vec3 inMax, const Mat4x4& inTransform = Mat4x4(1.0f));

    uint64_t GetDisplayTexture();
    uint64_t GetImGuiTextureID(uint32_t inTextureID) { return inTextureID; }

    uint32_t    GetDebugTextureCount() const override;
    const char* GetDebugTextureName(uint32_t inIndex) const override;

    uint32_t GetScreenshotBuffer(uint8_t* ioBuffer);
    uint32_t GetSelectedEntity(uint32_t inScreenPosX, uint32_t inScreenPosY) { return m_GBuffer ? m_GBuffer->readEntity(inScreenPosX, inScreenPosY) : NULL_ENTITY; }
    
    void OnResize(const Viewport& inViewport) { 
        CreateRenderTargets(inViewport);
    }

    void DrawImGui(Scene& inScene, const Viewport& inViewport);

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
    std::shared_ptr<ImGuiPass>                  m_ImGuiPass;
    std::shared_ptr<ShadowMap>                  m_ShadowMaps;
    std::shared_ptr<Atmosphere>                 m_Atmosphere;
    std::shared_ptr<TAAResolve>                 m_ResolveTAA;
    std::shared_ptr<DebugLines>                 m_DebugLines;
    std::shared_ptr<VoxelizeDebug>              m_DebugVoxels;
    std::shared_ptr<DeferredShading>            m_DeferredShading;
    std::vector<std::shared_ptr<RenderPass>>    m_RenderPasses;

private:
    uint32_t      m_FrameNr = 0;
    uint32_t      m_DefaultBlackTexture = 0;
    SDL_Window*   m_Window = nullptr;
    SDL_GLContext m_GLContext = nullptr;
};

} // namespace Raekor