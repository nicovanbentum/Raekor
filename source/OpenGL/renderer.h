#pragma once

#include "renderpass.h"
#include "application.h"

namespace RK {
class Scene;
class Async;
class Viewport;
}

namespace RK::GL {


class Renderer : public IRenderInterface
{
public:
    Renderer(SDL_Window* inWindow, Viewport& ioViewport);
    ~Renderer();

    void Render(const Application* inApp, const Scene& inScene, const Viewport& inViewport);
    void CreateRenderTargets(const Viewport& inViewport);

    uint64_t GetLightTexture() { return Icons::lightTexture; };
    uint64_t GetDisplayTexture();
    uint64_t GetImGuiTextureID(uint32_t inTextureID) { return inTextureID; }

    uint32_t    GetDebugTextureCount() const override;
    const char* GetDebugTextureName(uint32_t inIndex) const override;

    uint32_t GetScreenshotBuffer(uint8_t* ioBuffer);
    uint32_t GetSelectedEntity(const Scene& inScene, uint32_t inScreenPosX, uint32_t inScreenPosY) { return m_GBuffer ? m_GBuffer->readEntity(inScreenPosX, inScreenPosY) : Entity::Null; }

    void OnResize(const Viewport& inViewport) { CreateRenderTargets(inViewport); }

    void DrawDebugSettings(Application* inApp, Scene& inScene, const Viewport& inViewport);

    void UploadMeshBuffers(Entity inEntity, Mesh& inMesh) override;
    void DestroyMeshBuffers(Entity inEntity, Mesh& inMesh);

    void UploadSkeletonBuffers(Entity inEntity, Skeleton& inSkeleton, Mesh& inMesh) override;
    void DestroySkeletonBuffers(Entity inEntity, Skeleton& inSkeleton);

    void DestroyMaterialTextures(Entity inEntity, Material& inMaterial, Assets& inAssets);

    uint32_t UploadTextureFromAsset(const TextureAsset::Ptr& inAsset, bool inIsSRGB = false, uint8_t inSwizzle = TEXTURE_SWIZZLE_RGBA) override;

public:
    SharedPtr<Bloom>              m_Bloom;
    SharedPtr<Icons>              m_Icons;
    SharedPtr<Tonemap>            m_Tonemap;
    SharedPtr<GBuffer>            m_GBuffer;
    SharedPtr<Voxelize>           m_Voxelize;
    SharedPtr<Skinning>           m_Skinning;
    SharedPtr<ImGuiPass>          m_ImGuiPass;
    SharedPtr<ShadowMap>          m_ShadowMaps;
    SharedPtr<Atmosphere>         m_Atmosphere;
    SharedPtr<TAAResolve>         m_ResolveTAA;
    SharedPtr<DebugLines>         m_DebugLines;
    SharedPtr<VoxelizeDebug>      m_DebugVoxels;
    SharedPtr<DeferredShading>    m_DeferredShading;
    Array<SharedPtr<RenderPass>>  m_RenderPasses;

private:
    uint32_t      m_FrameNr = 0;
    uint32_t      m_FrameBuffer = 0;
    uint32_t      m_DefaultBlackTexture = 0;
    SDL_Window* m_Window = nullptr;
    SDL_GLContext m_GLContext = nullptr;
};

} // namespace Raekor