#pragma once

#include "Shared.h"
#include "Resource.h"
#include "GPUScene.h"
#include "Upscalers.h"
#include "RenderGraph.h"
#include "RenderWorld.h"
#include "RenderPasses.h"

#include "Threading.h"
#include "Components.h"
#include "Application.h"

namespace RK {

class Application;
class Scene;

}

namespace RK::DX12 {

class Device;
class CommandList;
class RenderGraph;


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
        int& mEnableDebugOverlay = g_CVariables->Create("r_enable_debug_overlay", 1, true);
        int& mEnableRTAO         = g_CVariables->Create("r_enable_rtao",          0, true);
        int& mEnableGTAO         = g_CVariables->Create("r_enable_gtao",          0, true);
        int& mEnableSSR          = g_CVariables->Create("r_enable_ssr",           0, true);
        int& mEnableShadows      = g_CVariables->Create("r_enable_shadows",       0);
        int& mEnableReflections  = g_CVariables->Create("r_enable_reflections",   0);
        int& mEnableAutoExposure = g_CVariables->Create("r_enable_auto_exposure", 0);
        int& mFullscreen         = g_CVariables->Create("r_fullscreen",           0);
        int& mDisplayMode        = g_CVariables->Create("r_display_mode",         0);
        int& mEnableTAA          = g_CVariables->Create("r_enable_taa",           1);
        int& mEnableDoF          = g_CVariables->Create("r_enable_dof",           0);
        int& mEnableBloom        = g_CVariables->Create("r_enable_bloom",         0);
        int& mEnableVignette     = g_CVariables->Create("r_enable_vignette",      0);
        int& mDoPathTrace        = g_CVariables->Create("r_path_trace",           0,   true);
        float& mSunConeAngle     = g_CVariables->Create("r_sun_cone_angle",       0.0f, true);
        float& mJitterScale      = g_CVariables->Create("r_jitter_scale",         1.0f, true);
        int& mDebugTexture       = g_CVariables->Create("r_debug_texture",        0,   true);
    } m_Settings;

public:
    Renderer(Device& inDevice, const Viewport& inViewport, SDL_Window* inWindow);

    void OnResize(Device& inDevice, Viewport& inViewport, bool inExclusiveFullscreen = false);
    void OnResizeViewport(Device& inDevice, Viewport& inViewport);
    void OnRender(Application* inApp, Device& inDevice, Viewport& inViewport, const Scene& inScene, float inDeltaTime);

    void Recompile(Device& inDevice);

    CommandList& StartSingleSubmit();
    void FlushSingleSubmit(Device& inDevice, CommandList& inCommandList);

    void WaitForIdle(Device& inDevice);

    void SetShouldResize(bool inValue) { m_ShouldResize = inValue; }
    void SetShouldRecompile(bool inValue) { m_ShouldRecompile = inValue; }
    void SetShouldCaptureNextFrame(bool inValue) { m_ShouldCaptureNextFrame = inValue; }

    void SetDefaultTextures(TextureID inBlackTexture, TextureID inWhiteTexture) { m_BlackTexture = inBlackTexture; m_WhiteTexture = inWhiteTexture; }
    void CreateDisplayTextureProxy(Device& inDevice);

    GPUScene& GetGPUScene() { return m_GPUScene; }
    const RenderWorld& GetRenderWorld() const { return m_RenderWorld; }

    TextureID GetEntityTexture() const;
    TextureID GetDisplayTexture() const;
    DescriptorID GetDisplayTextureProxy() const { return m_DisplayTextureProxy; }

    uint64_t RequestEntityPick(UVec2 inPixel);
    bool GetEntityPickResult(uint64_t inRequestID, Entity& outEntity) const;

    void RequestScreenshot(const Path& inFile) { m_PendingScreenshot = inFile; }

    SDL_Window*         GetWindow() const       { return m_Window; }
    Settings&           GetSettings()           { return m_Settings; }
    const Settings&     GetSettings() const     { return m_Settings; }
    Upscaler&           GetUpscaler()           { return m_Upscaler; }
    const RenderGraph&  GetRenderGraph() const  { return m_RenderGraph; }
    uint64_t            GetFrameCounter() const { return m_FrameCounter; }
    BackBufferData&     GetBackBufferData()     { return m_BackBufferData[m_FrameIndex]; }
    BackBufferData&     GetPrevBackBufferData() { return m_BackBufferData[m_PrevFrameIndex]; }

public:
    static constexpr uint32_t sFrameCount = 2;
    static constexpr DXGI_FORMAT sSwapchainFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

private:
    static constexpr float sDDGIHysteresis = 0.97f;
    static constexpr float sDDGIFastConvergeHysteresis = 0.85f;
    static constexpr uint32_t sDDGIFastConvergeFrameCount = 30;

    void CreateProbeDebugMesh(Device& inDevice);
    void UpdateFontAtlas(Device& inDevice);
    void ResolveImGuiDisplayTexture(Device& inDevice);

    uint64_t GetViewportKey(const Viewport& inViewport) const;
    uint64_t GetRenderGraphKey() const;
    uint64_t GetLightingKey() const;
    uint64_t GetPathTraceKey(uint64_t inLightingKey) const;

    void ResolveReadbacks(Device& inDevice);
    void RecordReadbacks(Device& inDevice, CommandList& inCmdList);

    struct Readback
    {
        BufferID    mBuffer;
        Path        mFile;
        uint64_t    mRequestID = 0;
        UVec2       mSize = UVec2(0, 0);
        uint32_t    mRowPitch = 0;
        DXGI_FORMAT mFormat = DXGI_FORMAT_UNKNOWN;
    };

private:
    SDL_Window*                 m_Window;
    RenderGraphResourceID       m_EntityTexture;
    RenderGraphResourceID       m_DisplayResource;
    RenderGraphResourceViewID   m_DisplayTexture;
    DescriptorID                m_DisplayTextureProxy;
    UVec2                       m_PendingEntityPickPixel;
    uint64_t                    m_PendingEntityPickID = 0;
    uint64_t                    m_EntityPickRequestCounter = 0;
    uint64_t                    m_EntityPickResultID = 0;
    Entity                      m_EntityPickResult = Entity::Null;
    Path                        m_PendingScreenshot;
    Readback                    m_EntityPickReadbacks[sFrameCount];
    Readback                    m_ScreenshotReadbacks[sFrameCount];
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
    uint64_t                    m_ViewportKey = 0;
    uint64_t                    m_RenderGraphKey = 0;
    uint64_t                    m_PathTraceKey = 0;
    uint64_t                    m_LightingKey = 0;
    uint32_t                    m_DDGIFastConvergeFrames = 0;
    uint32_t                    m_DDGIRelocationFrames = 0;
    Array<RTGeometry>           m_PrevGeometries;
    BufferID                    m_DebugLinesVertexBuffer;
    BufferID                    m_DebugLinesIndirectArgsBuffer;
    Mesh                        m_ProbeDebugMesh;
    TextureID                   m_BlackTexture;
    TextureID                   m_WhiteTexture;
    TextureID                   m_FontAtlasTexture;
    uint32_t                    m_FontAtlasVersion = 0;
    BackBufferData              m_BackBufferData[sFrameCount];
    FrameConstants              m_FrameConstants = {};
    GlobalConstants             m_GlobalConstants = {};
    Upscaler                    m_Upscaler;
    GPUScene                    m_GPUScene;
    RenderWorld                 m_RenderWorld;
    RenderGraph                 m_RenderGraph;
};



/* Initializes ImGui and returns the font texture ID. */
[[nodiscard]] TextureID InitImGui(Device& inDevice, DXGI_FORMAT inRtvFormat, uint32_t inFrameCount);

/* Renders ImGui directly to the backbuffer. */
void RenderImGui(RenderGraph& inRenderGraph, Device& inDevice, CommandList& inCmdList, TextureID inBackBuffer);


} // namespace Raekor