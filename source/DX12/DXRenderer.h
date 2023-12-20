#pragma once

#include "shared.h"
#include "DXResource.h"
#include "DXUpscaler.h"
#include "DXRenderGraph.h"
#include "DXRenderPasses.h"

#include "Raekor/async.h"
#include "Raekor/application.h"

namespace Raekor {

class Application;

}

namespace Raekor::DX12 {

class Device;
class CommandList;
class RenderGraph;
class RayTracedScene;


struct BackBufferData
{
    uint64_t    mFenceValue = 0;
    TextureID   mBackBuffer;
    CommandList mCopyCmdList;
    CommandList mDirectCmdList;
};


/*
    Fun TODO's:
    - timestamp queries per render pass for profiling
    - debug UI to reroute any intermediate texture to screen
*/
class Renderer
{
private:
    struct Settings
    {
        int& mEnableImGui    = g_CVars.Create("r_enable_imgui",        1);
        int& mEnableVsync    = g_CVars.Create("r_vsync",               1);
        int& mDebugLines     = g_CVars.Create("r_debug_lines",         1);
        int& mEnableDDGI     = g_CVars.Create("r_enable_ddgi",         1);
        int& mEnableRTAO     = g_CVars.Create("r_enable_rtao",         1);
        int& mProbeDebug     = g_CVars.Create("r_debug_gi_probes",     0);
        int& mFullscreen     = g_CVars.Create("r_fullscreen",          0);
        int& mDisplayRes     = g_CVars.Create("r_display_resolution",  0);
        int& mEnableTAA      = g_CVars.Create("r_enable_taa",          0);
        int& mEnableDoF      = g_CVars.Create("r_enable_dof",          1);
        int& mEnableBloom    = g_CVars.Create("r_enable_bloom",        1);
        int& mUpscaler       = g_CVars.Create("r_upscaler",            0,    true);
        int& mUpscaleQuality = g_CVars.Create("r_upscaler_quality",    0,    true);
        int& mDoPathTrace    = g_CVars.Create("r_path_trace",          0,    true);
        float& mSunConeAngle = g_CVars.Create("r_sun_cone_angle",      0.0f, true);
    } m_Settings;

public:
    Renderer(Device& inDevice, const Viewport& inViewport, SDL_Window* inWindow);

    void OnResize(Device& inDevice, Viewport& inViewport, bool inExclusiveFullscreen = false);
    void OnRender(Application* inApp, Device& inDevice, Viewport& inViewport, RayTracedScene& inScene, StagingHeap& inStagingHeap, IRenderInterface* inRenderInterfacee, float inDeltaTime);

    void Recompile(Device& inDevice, const RayTracedScene& inScene, IRenderInterface* inRenderInterface);

    CommandList& StartSingleSubmit();
    void FlushSingleSubmit(Device& inDevice, CommandList& inCommandList);

    void WaitForIdle(Device& inDevice);

    void SetShouldResize(bool inValue) { m_ShouldResize = inValue; }
    void SetShouldCaptureNextFrame(bool inValue) { m_ShouldCaptureNextFrame = inValue; }

    bool InitFSR(Device& inDevice, const Viewport& inViewport);
    bool DestroyFSR(Device& inDevice);

    bool InitDLSS(Device& inDevice, const Viewport& inViewport);
    bool DestroyDLSS(Device& inDevice);

    bool InitXeSS(Device& inDevice, const Viewport& inViewport);
    bool DestroyXeSS(Device& inDevice);

    void QueueMeshUpload(Entity inEntity) { m_PendingMeshUploads.push_back(inEntity); }
    void QueueMaterialUpload(Entity inEntity) { m_PendingMaterialUploads.push_back(inEntity); }

    void QueueTextureUpload(TextureID inTexture, uint32_t inMip, const Slice<char>& inData) { m_PendingTextureUploads.emplace_back(TextureUpload{ inMip, inTexture, inData }); }
    void QueueTextureUpload(TextureID inTexture, uint32_t inMip, const TextureAsset::Ptr& inData) { m_PendingTextureUploads.emplace_back(TextureUpload{ inMip, inTexture, Slice(inData->GetData(), inData->GetDataSize()) }); }

    SDL_Window*         GetWindow() const       { return m_Window; }
    Settings&           GetSettings()           { return m_Settings; }
    const RenderGraph&  GetRenderGraph() const  { return m_RenderGraph; }
    uint64_t            GetFrameCounter() const { return m_FrameCounter; }
    BackBufferData&     GetBackBufferData()     { return m_BackBufferData[m_FrameIndex]; }
    BackBufferData&     GetPrevBackBufferData() { return m_BackBufferData[!m_FrameIndex]; }

public:
    static constexpr uint32_t sFrameCount = 2;
    static constexpr DXGI_FORMAT sSwapchainFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

private:
    SDL_Window* m_Window;
    std::vector<Entity>         m_PendingMeshUploads;
    std::vector<Entity>         m_PendingMaterialUploads;
    std::vector<TextureUpload>  m_PendingTextureUploads;
    Job::Ptr                    m_PresentJobPtr;
    uint32_t                    m_FrameIndex;
    float                       m_ElapsedTime = 0;
    ComPtr<IDXGISwapChain3>     m_Swapchain;
    ComPtr<ID3D12Fence>         m_Fence;
    HANDLE                      m_FenceEvent;
    uint64_t                    m_FrameCounter = 0;
    bool                        m_ShouldResize = false;
    bool                        m_ShouldCaptureNextFrame = false;
    BackBufferData              m_BackBufferData[sFrameCount];
    FrameConstants              m_FrameConstants = {};
    GlobalConstants             m_GlobalConstants = {};
    FfxFsr2Context              m_Fsr2Context;
    std::vector<uint8_t>        m_FsrScratchMemory;
    NVSDK_NGX_Handle*           m_DLSSHandle = nullptr;
    NVSDK_NGX_Parameter*        m_DLSSParams = nullptr;
    xess_context_handle_t       m_XeSSContext = nullptr;
    RenderGraph                 m_RenderGraph;
};



class RenderInterface : public IRenderInterface
{
public:
    RenderInterface(Device& inDevice, Renderer& inRenderer, const RenderGraphResources& inResources, StagingHeap& inStagingHeap);

    void UpdateGPUStats(Device& inDevice);

    uint64_t GetDisplayTexture() override;
    uint64_t GetImGuiTextureID(uint32_t inTextureID) override;

    virtual uint32_t    GetDebugTextureCount() const override { return DEBUG_TEXTURE_COUNT; }
    virtual const char* GetDebugTextureName(uint32_t inIndex) const;

    uint32_t GetScreenshotBuffer(uint8_t* ioBuffer) { return 0; }
    uint32_t GetSelectedEntity(uint32_t inScreenPosX, uint32_t inScreenPosY) override { return NULL_ENTITY; /* TODO: FIXME */ }

    void UploadMeshBuffers(Entity inEntity, Mesh& inMesh) override;
    void DestroyMeshBuffers(Entity inEntity, Mesh& inMesh) override { /* TODO: FIXME */ }

    void UploadSkeletonBuffers(Skeleton& inSkeleton, Mesh& inMesh) override { /* TODO: FIXME */ }
    void DestroySkeletonBuffers(Skeleton& inSkeleton) override { /* TODO: FIXME */ }

    void UploadMaterialTextures(Entity inEntity, Material& inMaterial, Assets& inAssets) override;
    void DestroyMaterialTextures(Entity inEntity, Material& inMaterial, Assets& inAssets) override {}

    uint32_t UploadTextureFromAsset(const TextureAsset::Ptr& inAsset, bool inIsSRGB = false, uint8_t inSwizzle = TEXTURE_SWIZZLE_RGBA) override;

    void OnResize(const Viewport& inViewport) override;
    void DrawImGui(Scene& inScene, const Viewport& inViewport) override {}

private:
    Device& m_Device;
    Renderer& m_Renderer;
    StagingHeap& m_StagingHeap;
    const RenderGraphResources& m_Resources;
};



/* Initializes ImGui and returns the font texture ID. */
[[nodiscard]] TextureID InitImGui(Device& inDevice, DXGI_FORMAT inRtvFormat, uint32_t inFrameCount);

/* Renders ImGui directly to the backbuffer. */
void RenderImGui(RenderGraph& inRenderGraph, Device& inDevice, CommandList& inCmdList, TextureID inBackBuffer);


} // namespace Raekor