#pragma once

#include "shared.h"
#include "DXResource.h"
#include "DXRenderGraph.h"

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

enum EDebugTexture
{
    DEBUG_TEXTURE_NONE = 0,
    DEBUG_TEXTURE_GBUFFER_DEPTH = 1,
    DEBUG_TEXTURE_GBUFFER_ALBEDO = 2,
    DEBUG_TEXTURE_GBUFFER_NORMALS = 3,
    DEBUG_TEXTURE_GBUFFER_VELOCITY = 4,
    DEBUG_TEXTURE_GBUFFER_METALLIC = 5,
    DEBUG_TEXTURE_GBUFFER_ROUGHNESS = 6,
    DEBUG_TEXTURE_RT_AO = 7,
    DEBUG_TEXTURE_RT_SHADOWS = 8,
    DEBUG_TEXTURE_RT_REFLECTIONS = 9,
    DEBUG_TEXTURE_LIGHTING = 10,
    DEBUG_TEXTURE_COUNT
};

enum EUpscaler
{
    UPSCALER_NONE,
    UPSCALER_FSR2,
    UPSCALER_DLSS,
    UPSCALER_XESS,
    UPSCALER_COUNT
};

struct DDGISceneSettings
{
    RTTI_DECLARE_TYPE(DDGISceneSettings);

    IVec3 mDDGIDebugProbe = UVec3(0, 0, 0);
    IVec3 mDDGIProbeCount = UVec3(2, 2, 2);
    Vec3 mDDGIProbeSpacing = Vec3(0.1f, 0.1f, 0.1f);
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
        int& mEnableImGui = g_CVars.Create("r_enable_imgui",        1);
        int& mEnableVsync = g_CVars.Create("r_vsync",               1);
        int& mDebugLines  = g_CVars.Create("r_debug_lines",         1);
        int& mEnableDDGI  = g_CVars.Create("r_enable_ddgi",         1);
        int& mEnableRTAO  = g_CVars.Create("r_enable_rtao",         1);
        int& mProbeDebug  = g_CVars.Create("r_debug_gi_probes",     0);
        int& mFullscreen  = g_CVars.Create("r_fullscreen",          0);
        int& mDisplayRes  = g_CVars.Create("r_display_resolution",  0);
        int& mEnableTAA   = g_CVars.Create("r_enable_taa",          0);
        int& mUpscaler    = g_CVars.Create("r_upscaler",            0, true);
        int& mDoPathTrace = g_CVars.Create("r_path_trace",          0, true);
    } m_Settings;

public:
    Renderer(Device& inDevice, const Viewport& inViewport, SDL_Window* inWindow);

    void OnResize(Device& inDevice, const Viewport& inViewport, bool inExclusiveFullscreen = false);
    void OnRender(Application* inApp, Device& inDevice, const Viewport& inViewport, RayTracedScene& inScene, StagingHeap& inStagingHeap, IRenderInterface* inRenderInterfacee, float inDeltaTime);

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


////////////////////////////////////////
/// GBuffer Render Pass
////////////////////////////////////////
struct GBufferData
{
    RTTI_DECLARE_TYPE(GBufferData);

    Entity mActiveEntity = NULL_ENTITY;
    RenderGraphResourceID mDepthTexture;
    RenderGraphResourceID mRenderTexture;
    RenderGraphResourceID mVelocityTexture;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const GBufferData& AddGBufferPass(RenderGraph& inRenderGraph, Device& inDevice,
    const Scene& inScene
);


//////////////////////////////////////////
///// GBuffer Debug Pass
//////////////////////////////////////////
struct GBufferDebugData
{
    RTTI_DECLARE_TYPE(GBufferDebugData);

    GBufferData mGBufferData;
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mInputTextureSRV;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const GBufferDebugData& AddGBufferDebugPass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferData& inGBufferData,
    EDebugTexture inDebugTexture
);


//////////////////////////////////////////
///// Grass Pass
//////////////////////////////////////////
struct GrassData
{
    RTTI_DECLARE_TYPE(GrassData);

    BufferID mPerBladeIndexBuffer;
    RenderGraphResourceViewID mDepthTextureSRV;
    RenderGraphResourceViewID mRenderTextureSRV;
    GrassRenderRootConstants mRenderConstants;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const GrassData& AddGrassRenderPass(RenderGraph& inGraph, Device& inDevice,
    const GBufferData& inGBufferData
);


////////////////////////////////////////
/// Ray-traced Shadow Mask Render Pass
////////////////////////////////////////
struct RTShadowMaskData
{
    RTTI_DECLARE_TYPE(RTShadowMaskData);

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mGbufferDepthTextureSRV;
    RenderGraphResourceViewID mGBufferRenderTextureSRV;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const RTShadowMaskData& AddShadowMaskPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene,
    const GBufferData& inGBufferData
);


//////////////////////////////////////////
///// Ray-traced Ambient Occlusion Render Pass
//////////////////////////////////////////
struct RTAOData
{
    RTTI_DECLARE_TYPE(RTAOData);

    static inline AmbientOcclusionParams mParams = 
    { 
        .mRadius = 2.0, 
        .mPower = 0.0, 
        .mNormalBias = 0.01, 
        .mSampleCount = 1u 
    };

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mGbufferDepthTextureSRV;
    RenderGraphResourceViewID mGBufferRenderTextureSRV;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const RTAOData& AddAmbientOcclusionPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene,
    const GBufferData& inGBufferData
);


//////////////////////////////////////////
///// Ray-traced Reflections Render Pass
//////////////////////////////////////////
struct ReflectionsData
{
    RTTI_DECLARE_TYPE(ReflectionsData);

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mGBufferDepthTextureSRV;
    RenderGraphResourceViewID mGbufferRenderTextureSRV;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ReflectionsData& AddReflectionsPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene,
    const GBufferData& inGBufferData
);


//////////////////////////////////////////
///// Ray-traced Indirect Diffuse Render Pass
//////////////////////////////////////////
struct PathTraceData
{
    RTTI_DECLARE_TYPE(PathTraceData);

    static inline float mAlpha = 0.1f;
    static inline uint32_t mBounces = 2;
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceID mAccumulationTexture;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const PathTraceData& AddPathTracePass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene
);


//////////////////////////////////////////
///// Downsample Render Pass
//////////////////////////////////////////
struct DownsampleData
{
    RTTI_DECLARE_TYPE(DownsampleData);

    RenderGraphResourceID mGlobalAtomicBuffer;
    RenderGraphResourceViewID mSourceTextureUAV;
    RenderGraphResourceViewID mSourceTextureMipsUAVs[12];
    ComPtr<ID3D12PipelineState> mPipeline;
};

const DownsampleData& AddDownsamplePass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID inSourceTexture
);


//////////////////////////////////////////
///// GI Probe Trace Compute Pass
//////////////////////////////////////////
struct ProbeTraceData
{
    RTTI_DECLARE_TYPE(ProbeTraceData);

    ProbeTraceData()
    {
        mDDGIData.mCornerPosition = Vec3(-65, -1.4, -28.5);
        mDDGIData.mProbeCount = IVec3(22, 22, 22);
        mDDGIData.mProbeSpacing = Vec3(6.4, 2.8, 2.8);
    }

    IVec3           mDebugProbe = IVec3(10, 10, 5);
    DDGIData        mDDGIData;
    Mat3x3          mRandomRotationMatrix;
    RenderGraphResourceID mProbesDepthTexture;
    RenderGraphResourceID mProbesIrradianceTexture;
    RenderGraphResourceID mRaysDepthTexture;
    RenderGraphResourceID mRaysIrradianceTexture;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ProbeTraceData& AddProbeTracePass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene
);


//////////////////////////////////////////
///// GI Probe Update Compute Pass
//////////////////////////////////////////
struct ProbeUpdateData
{
    RTTI_DECLARE_TYPE(ProbeUpdateData);

    DDGIData        mDDGIData;
    RenderGraphResourceID mProbesDepthTexture;
    RenderGraphResourceID mProbesIrradianceTexture;
    RenderGraphResourceViewID mRaysDepthTextureSRV;
    RenderGraphResourceViewID mRaysIrradianceTextureSRV;
    ComPtr<ID3D12PipelineState> mDepthPipeline;
    ComPtr<ID3D12PipelineState> mClearPipeline;;
    ComPtr<ID3D12PipelineState> mIrradiancePipeline;
};

const ProbeUpdateData& AddProbeUpdatePass(RenderGraph& inRenderGraph, Device& inDevice,
    const ProbeTraceData& inTraceData
);



//////////////////////////////////////////
///// GI Probe Debug Render Pass
//////////////////////////////////////////
struct ProbeDebugData
{
    RTTI_DECLARE_TYPE(ProbeDebugData);

    Mesh            mProbeMesh;
    DDGIData        mDDGIData;
    RenderGraphResourceViewID mRenderTargetRTV;
    RenderGraphResourceViewID mDepthTargetDSV;
    RenderGraphResourceViewID mProbesDepthTextureSRV;
    RenderGraphResourceViewID mProbesIrradianceTextureSRV;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ProbeDebugData& AddProbeDebugPass(RenderGraph& inRenderGraph, Device& inDevice,
    const ProbeTraceData& inTraceData,
    const ProbeUpdateData& inUpdateData,
    RenderGraphResourceID inRenderTarget,
    RenderGraphResourceID inDepthTarget
);


//////////////////////////////////////////
///// Debug Lines Render Pass
//////////////////////////////////////////
struct DebugLinesData
{
    RTTI_DECLARE_TYPE(DebugLinesData);

    D3D12_DRAW_ARGUMENTS* mMappedPtr;
    RenderGraphResourceID mVertexBuffer;
    RenderGraphResourceID mIndirectArgsBuffer;
    RenderGraphResourceID mVertexBufferSRV;
    RenderGraphResourceID mIndirectArgsBufferSRV;
    ComPtr<ID3D12PipelineState> mPipeline;
    ComPtr<ID3D12CommandSignature> mCommandSignature;
};

const DebugLinesData& AddDebugLinesPass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID inRenderTarget,
    RenderGraphResourceID inDepthTarget
);


//////////////////////////////////////////
///// Deferred Lighting Render Pass
//////////////////////////////////////////
struct LightingData
{
    RTTI_DECLARE_TYPE(LightingData);

    DDGIData        mDDGIData;
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mShadowMaskTextureSRV;
    RenderGraphResourceViewID mReflectionsTextureSRV;
    RenderGraphResourceViewID mGBufferDepthTextureSRV;
    RenderGraphResourceViewID mGBufferRenderTextureSRV;
    RenderGraphResourceViewID mAmbientOcclusionTextureSRV;
    RenderGraphResourceViewID mProbesDepthTextureSRV;
    RenderGraphResourceViewID mProbesIrradianceTextureSRV;
    RenderGraphResourceViewID mIndirectDiffuseTextureSRV;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const LightingData& AddLightingPass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferData& inGBufferData,
    const RTShadowMaskData& inShadowMaskData,
    const ReflectionsData& inReflectionsData,
    RenderGraphResourceID inAOTexture,
    const ProbeUpdateData& inProbeData
);


//////////////////////////////////////////
///// AMD FSR 2.1 Render Pass
//////////////////////////////////////////
struct FSR2Data
{
    RTTI_DECLARE_TYPE(FSR2Data);

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mColorTextureSRV;
    RenderGraphResourceViewID mDepthTextureSRV;
    RenderGraphResourceViewID mMotionVectorTextureSRV;
    float mDeltaTime = 0;
    uint32_t mFrameCounter = 0;
};

const FSR2Data& AddFsrPass(RenderGraph& inRenderGraph, Device& inDevice,
    FfxFsr2Context& inContext,
    RenderGraphResourceID inColorTexture,
    const GBufferData& inGBufferData
);


//////////////////////////////////////////
///// Nvidia DLSS Render Pass
//////////////////////////////////////////
struct DLSSData
{
    RTTI_DECLARE_TYPE(DLSSData);

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mColorTextureSRV;
    RenderGraphResourceViewID mDepthTextureSRV;
    RenderGraphResourceViewID mMotionVectorTextureSRV;
    uint32_t mFrameCounter = 0;
};

const DLSSData& AddDLSSPass(RenderGraph& inRenderGraph, Device& inDevice,
    NVSDK_NGX_Handle* inDLSSHandle, 
    NVSDK_NGX_Parameter* inDLSSParams,
    RenderGraphResourceID inColorTexture,
    const GBufferData& inGBufferData
);


//////////////////////////////////////////
///// Intel XeSS Render Pass
//////////////////////////////////////////
struct XeSSData
{
    RTTI_DECLARE_TYPE(XeSSData);

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mColorTextureSRV;
    RenderGraphResourceViewID mDepthTextureSRV;
    RenderGraphResourceViewID mMotionVectorTextureSRV;
    uint32_t mFrameCounter = 0;
};

const XeSSData& AddXeSSPass(RenderGraph& inRenderGraph, Device& inDevice,
    xess_context_handle_t inContext,
    RenderGraphResourceID inColorTexture,
    const GBufferData& inGBufferData
);


//////////////////////////////////////////
///// TAA Resolve Render Pass
//////////////////////////////////////////
struct TAAResolveData
{
    RTTI_DECLARE_TYPE(TAAResolveData);

    uint32_t mFrameCounter = 0;
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceID mHistoryTexture;
    RenderGraphResourceViewID mHistoryTextureSRV;
    RenderGraphResourceViewID mColorTextureSRV;
    RenderGraphResourceViewID mDepthTextureSRV;
    RenderGraphResourceViewID mVelocityTextureSRV;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const TAAResolveData& AddTAAResolvePass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferData& inGBufferData,
    RenderGraphResourceID inColorTexture,
    uint32_t inFrameCounter
);


////////////////////////////////////////
/// Final Compose Render Pass
////////////////////////////////////////
struct ComposeData
{
    RTTI_DECLARE_TYPE(ComposeData);

    static inline float mExposure = 1.0f;
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mInputTextureSRV;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ComposeData& AddComposePass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID inInputTexture
);


////////////////////////////////////////
/// Pre-ImGui Pass
////////////////////////////////////////
struct PreImGuiData
{
    RTTI_DECLARE_TYPE(PreImGuiData);

    RenderGraphResourceViewID mDisplayTextureSRV;
};

const PreImGuiData& AddPreImGuiPass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID ioDisplayTexture
);


////////////////////////////////////////
/// ImGui Pass
////////////////////////////////////////
struct ImGuiData
{
    RTTI_DECLARE_TYPE(ImGuiData);

    RenderGraphResourceID mIndexBuffer;
    RenderGraphResourceID mVertexBuffer;
    RenderGraphResourceViewID mBackBufferRTV;
    RenderGraphResourceViewID mInputTextureSRV;
    std::vector<uint8_t> mIndexScratchBuffer;
    std::vector<uint8_t> mVertexScratchBuffer;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ImGuiData& AddImGuiPass(RenderGraph& inRenderGraph, Device& inDevice,
    StagingHeap& inStagingHeap,
    RenderGraphResourceID inInputTexture,
    TextureID inBackBuffer
);

/* Initializes ImGui and returns the font texture ID. */
[[nodiscard]] TextureID InitImGui(Device& inDevice, DXGI_FORMAT inRtvFormat, uint32_t inFrameCount);

/* Renders ImGui directly to the backbuffer. */
void RenderImGui(RenderGraph& inRenderGraph, Device& inDevice, CommandList& inCmdList, TextureID inBackBuffer);

} // namespace Raekor