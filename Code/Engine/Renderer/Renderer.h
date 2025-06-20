#pragma once

#include "Shared.h"
#include "Resource.h"
#include "Upscalers.h"
#include "RenderGraph.h"
#include "RenderPasses.h"

#include "Threading.h"
#include "Application.h"

namespace RK {

class Application;
class Scene;

}

namespace RK::DX12 {

class Device;
class CommandList;
class RenderGraph;
class RayTracedScene;


struct BackBufferData
{
    uint64_t    mFenceValue = 0;
    TextureID   mBackBuffer;
    CommandList mCopyCmdList;
    CommandList mUpdateCmdList;
    CommandList mDirectCmdList;
};


class Renderer
{
private:
    struct Settings
    {
        int& mEnableImGui        = g_CVariables->Create("r_enable_imgui",         1);
        int& mEnableVsync        = g_CVariables->Create("r_vsync",                0);
        int& mTargetFps          = g_CVariables->Create("r_target_fps",          -1);
        int& mDisableAlbedo      = g_CVariables->Create("r_disable_albedo",       0, true);
        int& mEnableDDGI         = g_CVariables->Create("r_enable_ddgi",          1, true);
        int& mDebugProbeRays     = g_CVariables->Create("r_debug_gi_rays",        0, true);
        int& mDebugProbes        = g_CVariables->Create("r_debug_gi_probes",      0, true);
        int& mEnableDebugOverlay = g_CVariables->Create("r_enable_debug_overlay", 0);
        int& mEnableRTAO         = g_CVariables->Create("r_enable_rtao",          1, true);
        int& mEnableSSAO         = g_CVariables->Create("r_enable_ssao",          0, true);
        int& mEnableSSR          = g_CVariables->Create("r_enable_ssr",           0, true);
        int& mEnableShadows      = g_CVariables->Create("r_enable_shadows",       1);
        int& mEnableReflections  = g_CVariables->Create("r_enable_reflections",   0);
        int& mEnableAutoExposure = g_CVariables->Create("r_enable_auto_exposure", 0);
        int& mFullscreen         = g_CVariables->Create("r_fullscreen",           0);
        int& mDisplayMode        = g_CVariables->Create("r_display_mode",         0);
        int& mEnableTAA          = g_CVariables->Create("r_enable_taa",           1);
        int& mEnableDoF          = g_CVariables->Create("r_enable_dof",           0);
        int& mEnableBloom        = g_CVariables->Create("r_enable_bloom",         1);
        int& mEnableVignette     = g_CVariables->Create("r_enable_vignette",      1);
        int& mDoPathTrace        = g_CVariables->Create("r_path_trace",           0,   true);
        int& mDoPathTraceGBuffer = g_CVariables->Create("r_path_trace_gbuffer",   1,   true);
        float& mSunConeAngle     = g_CVariables->Create("r_sun_cone_angle",       0.0f, true);
    } m_Settings;

public:
    Renderer(Device& inDevice, const Viewport& inViewport, SDL_Window* inWindow);

    void OnResize(Device& inDevice, Viewport& inViewport, bool inExclusiveFullscreen = false);
    void OnRender(Application* inApp, Device& inDevice, Viewport& inViewport, RayTracedScene& inScene, IRenderInterface* inRenderInterfacee, float inDeltaTime);

    void Recompile(Device& inDevice, const RayTracedScene& inScene, IRenderInterface* inRenderInterface);

    CommandList& StartSingleSubmit();
    void FlushSingleSubmit(Device& inDevice, CommandList& inCommandList);

    void WaitForIdle(Device& inDevice);

    void SetShouldResize(bool inValue) { m_ShouldResize = inValue; }
    void SetShouldRecompile(bool inValue) { m_ShouldRecompile = inValue; }
    void SetShouldCaptureNextFrame(bool inValue) { m_ShouldCaptureNextFrame = inValue; }

    void QueueMeshUpload(Entity inEntity) { std::scoped_lock lock(m_UploadMutex); m_PendingMeshUploads.push_back(inEntity); }
    void QueueSkeletonUpload(Entity inEntity) { std::scoped_lock lock(m_UploadMutex); m_PendingSkeletonUploads.push_back(inEntity); }

    TextureID GetEntityTexture() const;
    TextureID GetDisplayTexture() const;

    SDL_Window*         GetWindow() const       { return m_Window; }
    Settings&           GetSettings()           { return m_Settings; }
    Upscaler&           GetUpscaler()           { return m_Upscaler; }
    const RenderGraph&  GetRenderGraph() const  { return m_RenderGraph; }
    uint64_t            GetFrameCounter() const { return m_FrameCounter; }
    BackBufferData&     GetBackBufferData()     { return m_BackBufferData[m_FrameIndex]; }
    BackBufferData&     GetPrevBackBufferData() { return m_BackBufferData[m_PrevFrameIndex]; }

public:
    static constexpr uint32_t sFrameCount = 2;
    static constexpr DXGI_FORMAT sSwapchainFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

private:
    SDL_Window*                 m_Window;
    Mutex                       m_UploadMutex;
    Array<Entity>               m_PendingMeshUploads;
    Array<TextureUpload>        m_PendingTextureUploads;
    Array<Entity>               m_PendingSkeletonUploads;
    RenderGraphResourceID       m_EntityTexture;
    RenderGraphResourceViewID   m_DisplayTexture;
    uint32_t                    m_FrameIndex = 0;
    uint32_t                    m_PrevFrameIndex = 0;
    uint64_t                    m_FrameCounter = 0;
    Job::Ptr                    m_PresentJobPtr = nullptr;
    float                       m_ElapsedTime = 0;
    ComPtr<ID3D12Fence>         m_Fence;
    HANDLE                      m_FenceEvent;
    ComPtr<IDXGISwapChain3>     m_Swapchain;
    bool                        m_ShouldResize = false;
    bool                        m_ShouldRecompile = false;
    bool                        m_ShouldCaptureNextFrame = false;
    BackBufferData              m_BackBufferData[sFrameCount];
    FrameConstants              m_FrameConstants = {};
    GlobalConstants             m_GlobalConstants = {};
    Upscaler                    m_Upscaler;
    RenderGraph                 m_RenderGraph;
};



class RenderInterface : public IRenderInterface
{
public:
    RenderInterface(Application* inApp, Device& inDevice, Renderer& inRenderer);

    void UpdateGPUStats(Device& inDevice);

    uint64_t GetLightTexture() override;
    uint64_t GetCameraTexture() override;
    uint64_t GetDisplayTexture() override;

    uint64_t GetImGuiTextureID(uint32_t inTextureID) override;

    uint64_t GetDebugTextureIndex() const override { return m_Settings.mDebugTexture; }
    void SetDebugTextureIndex(int inIndex) override { m_Settings.mDebugTexture = inIndex; }

    uint32_t GetDebugTextureCount() const override { return DEBUG_TEXTURE_COUNT; }
    const char* GetDebugTextureName(uint32_t inIndex) const override;

    uint32_t GetScreenshotBuffer(uint8_t* ioBuffer) { return 0; }

    void UploadMeshBuffers(Entity inEntity, Mesh& inMesh) override;
    void DestroyMeshBuffers(Entity inEntity, Mesh& inMesh) override;

    void UploadSkeletonBuffers(Entity inEntity, Skeleton& inSkeleton, Mesh& inMesh) override;
    void DestroySkeletonBuffers(Entity inEntity, Skeleton& inSkeleton) override;

    void UploadMaterialTextures(Entity inEntity, Material& inMaterial, Assets& inAssets) override;
    void DestroyMaterialTextures(Entity inEntity, Material& inMaterial, Assets& inAssets) override {}

    void CompileMaterialShaders(Entity inEntity, Material& inMaterial) override;
    void ReleaseMaterialShaders(Entity inEntity, Material& inMaterial) override;

    uint32_t UploadTextureFromAsset(TextureAsset::Ptr inAsset, bool inIsSRGB = false, uint8_t inSwizzle = TEXTURE_SWIZZLE_RGBA) override;

    uint32_t GetSelectedEntity(const Scene& inScene, uint32_t inScreenPosX, uint32_t inScreenPosY) override;

    void OnResize(const Viewport& inViewport) override;
    void DrawDebugSettings(Application* inApp, Scene& inScene, const Viewport& inViewport) override;

private:
    TextureID m_LightTexture;
    TextureID m_CameraTexture;

private:
    Device& m_Device;
    Viewport& m_Viewport;
    Renderer& m_Renderer;

};



/* Initializes ImGui and returns the font texture ID. */
[[nodiscard]] TextureID InitImGui(Device& inDevice, DXGI_FORMAT inRtvFormat, uint32_t inFrameCount);

/* Renders ImGui directly to the backbuffer. */
void RenderImGui(RenderGraph& inRenderGraph, Device& inDevice, CommandList& inCmdList, TextureID inBackBuffer);


} // namespace Raekor