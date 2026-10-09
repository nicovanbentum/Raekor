#pragma once

#include "Device.h"
#include "Renderer.h"
#include "RenderInterface.h"

namespace RK {

class Scene;
class Application;

}

namespace RK::DX12 {

class RenderSystem : public IRenderInterface
{
public:
    explicit RenderSystem(Application* inApp);
    ~RenderSystem();

    NO_COPY_NO_MOVE(RenderSystem);

    void InitImGui(SDL_Window* inWindow);
    void OnRender(Application* inApp, const Scene& inScene, float inDeltaTime);

    Device& GetDevice() { return m_Device; }
    Renderer& GetRenderer() { return m_Renderer; }

    uint32_t GetWhiteTexture() const override { return m_DefaultWhiteTexture.GetValue(); }
    uint32_t GetBlackTexture() const override { return m_DefaultBlackTexture.GetValue(); }

    uint64_t GetLightTexture() override;
    uint64_t GetCameraTexture() override;
    uint64_t GetDisplayTexture() override;
    uint64_t GetImGuiTextureID(uint32_t inTexture) override;

    uint32_t GetDebugTextureIndex() const override { return uint32_t(m_Renderer.GetSettings().mDebugTexture); }
    void SetDebugTextureIndex(uint32_t inIndex) override { m_Renderer.GetSettings().mDebugTexture = int(inIndex); }
    uint32_t GetDebugTextureCount() const override { return DEBUG_TEXTURE_COUNT; }
    const char* GetDebugTextureName(uint32_t inIndex) const override;

    void RequestScreenshot(const Path& inFile) override { m_Renderer.RequestScreenshot(inFile); }

    uint64_t RequestEntityPick(uint32_t inPixelX, uint32_t inPixelY) override { return m_Renderer.RequestEntityPick(UVec2(inPixelX, inPixelY)); }
    bool GetEntityPickResult(uint64_t inRequestID, Entity& outEntity) override { return m_Renderer.GetEntityPickResult(inRequestID, outEntity); }

    void UploadMeshBuffers(Entity inEntity, Mesh& inMesh) override;
    void DestroyMeshBuffers(Entity inEntity, Mesh& inMesh) override;
    void ShareMeshBuffers(Entity inEntity, const Mesh& inSource, Mesh& ioMesh) override;

    void UploadSkeletonBuffers(Entity inEntity, Skeleton& inSkeleton, Mesh& inMesh) override;
    void DestroySkeletonBuffers(Entity inEntity, Skeleton& inSkeleton) override;

    void CompileMaterialShaders(Entity inEntity, Material& inMaterial) override;
    void ReleaseMaterialShaders(Entity inEntity, Material& inMaterial) override;

    uint32_t UploadTextureFromAsset(TextureAsset::Ptr inAsset, bool inIsSRGB = false, uint8_t inSwizzle = TEXTURE_SWIZZLE_RGBA) override;

    void DrawDebugSettings(Application* inApp) override;

private:
    void CompileSystemShaders(SDL_Window* inWindow);
    void CreateDefaultTextures();
    void UpdateGPUStats();

private:
    Device m_Device;
    Renderer m_Renderer;

    TextureID m_DefaultWhiteTexture;
    TextureID m_DefaultBlackTexture;
    TextureID m_DefaultNormalTexture;
    TextureID m_BlueNoiseTexture;
    TextureID m_LightTexture;
    TextureID m_CameraTexture;
    TextureID m_ImGuiFontTexture;

    Mutex m_SharedMeshBuffersMutex;
    HashMap<uint32_t, uint32_t> m_SharedMeshBufferRefCounts;
};

} // namespace RK::DX12
