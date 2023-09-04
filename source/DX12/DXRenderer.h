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
    uint64_t    mFenceValue;
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
        int& mDoPathTrace = g_CVars.Create("r_path_trace",          0);
        int& mEnableVsync = g_CVars.Create("r_vsync",               1);
        int& mDebugLines  = g_CVars.Create("r_debug_lines",         1);
        int& mEnableRTAO  = g_CVars.Create("r_enable_rtao",         1);
        int& mProbeDebug  = g_CVars.Create("r_debug_gi_probes",     0);
        int& mEnableDDGI  = g_CVars.Create("r_enable_ddgi",         1);
        int& mFullscreen  = g_CVars.Create("r_fullscreen",          0);
        int& mEnableFsr2  = g_CVars.Create("r_enable_fsr2",         0);
        int& mDisplayRes  = g_CVars.Create("r_display_resolution",  0);
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

    SDL_Window* GetWindow()       const { return m_Window; }
    Settings& GetSettings() { return m_Settings; }
    const RenderGraph& GetRenderGraph()  const { return m_RenderGraph; }
    uint64_t            GetFrameCounter() const { return m_FrameCounter; }
    BackBufferData& GetBackBufferData() { return m_BackBufferData[m_FrameIndex]; }
    BackBufferData& GetPrevBackBufferData() { return m_BackBufferData[!m_FrameIndex]; }

public:
    static constexpr uint32_t sFrameCount = 2;
    static constexpr DXGI_FORMAT sSwapchainFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

private:
    SDL_Window* m_Window;
    Job::Ptr                m_PresentJobPtr;
    uint32_t                m_FrameIndex;
    float                   m_ElapsedTime = 0;
    ComPtr<IDXGISwapChain3> m_Swapchain;
    ComPtr<ID3D12Fence>     m_Fence;
    HANDLE                  m_FenceEvent;
    uint64_t                m_FrameCounter = 0;
    bool                    m_ShouldResize = false;
    bool                    m_ShouldCaptureNextFrame = false;
    BackBufferData          m_BackBufferData[sFrameCount];
    FrameConstants          m_FrameConstants;
    FfxFsr2Context          m_Fsr2Context;
    std::vector<uint8_t>    m_FsrScratchMemory;
    RenderGraph             m_RenderGraph;
};


class RenderInterface : public IRenderInterface
{
public:
    RenderInterface(Device& inDevice, Renderer& inRenderer, StagingHeap& inStagingHeap);

    uint64_t GetDisplayTexture() override;
    uint64_t GetImGuiTextureID(uint32_t inTextureID) override;

    virtual uint32_t    GetDebugTextureCount() const override { return DEBUG_TEXTURE_COUNT; }
    virtual const char* GetDebugTextureName(uint32_t inIndex) const;

    uint32_t GetScreenshotBuffer(uint8_t* ioBuffer) { return 0; /* TODO: FIXME */ }
    uint32_t GetSelectedEntity(uint32_t inScreenPosX, uint32_t inScreenPosY) override { return NULL_ENTITY; /* TODO: FIXME */ }

    void UploadMeshBuffers(Mesh& inMesh);
    void DestroyMeshBuffers(Mesh& inMesh) { /* TODO: FIXME */ }

    void UploadSkeletonBuffers(Skeleton& inSkeleton, Mesh& inMesh) { /* TODO: FIXME */ }
    void DestroySkeletonBuffers(Skeleton& inSkeleton) { /* TODO: FIXME */ }

    void DestroyMaterialTextures(Material& inMaterial, Assets& inAssets) override {}

    uint32_t UploadTextureFromAsset(const TextureAsset::Ptr& inAsset, bool inIsSRGB = false);

    void OnResize(const Viewport& inViewport);
    void DrawImGui(Scene& inScene, const Viewport& inViewport) {}

private:
    Device& m_Device;
    Renderer& m_Renderer;
    StagingHeap& m_StagingHeap;
};


////////////////////////////////////////
/// GBuffer Render Pass
////////////////////////////////////////
struct GBufferData
{
    RTTI_DECLARE_TYPE(GBufferData);

    Entity mActiveEntity = NULL_ENTITY;
    TextureResource mDepthTexture;
    TextureResource mRenderTexture;
    TextureResource mMotionVectorTexture;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const GBufferData& AddGBufferPass(RenderGraph& inRenderGraph, Device& inDevice,
    const Scene& inScene
);


////////////////////////////////////////
/// GBuffer Debug Pass
////////////////////////////////////////
struct GBufferDebugData
{
    RTTI_DECLARE_TYPE(GBufferDebugData);

    GBufferData mGBufferData;
    TextureResource mInputTexture;
    TextureResource mOutputTexture;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const GBufferDebugData& AddGBufferDebugPass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferData& inGBufferData,
    EDebugTexture inDebugTexture
);


////////////////////////////////////////
/// Grass Pass
////////////////////////////////////////
struct GrassData
{
    RTTI_DECLARE_TYPE(GrassData);

    BufferID mPerBladeIndexBuffer;
    TextureResource mDepthTexture;
    TextureResource mRenderTexture;
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

    TextureResource mOutputTexture;
    TextureResource mGbufferDepthTexture;
    TextureResource mGBufferRenderTexture;
    DescriptorID mTopLevelAccelerationStructure;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const RTShadowMaskData& AddShadowMaskPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene,
    const GBufferData& inGBufferData
);


////////////////////////////////////////
/// Ray-traced Ambient Occlusion Render Pass
////////////////////////////////////////
struct RTAOData
{
    RTTI_DECLARE_TYPE(RTAOData);

    AmbientOcclusionParams mParams = { .mRadius = 2.0, .mIntensity = 1.0, .mNormalBias = 0.01, .mSampleCount = 1u };
    TextureResource mOutputTexture;
    TextureResource mGbufferDepthTexture;
    TextureResource mGBufferRenderTexture;
    DescriptorID mTopLevelAccelerationStructure;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const RTAOData& AddAmbientOcclusionPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene,
    const GBufferData& inGBufferData
);


////////////////////////////////////////
/// Ray-traced Reflections Render Pass
////////////////////////////////////////
struct ReflectionsData
{
    RTTI_DECLARE_TYPE(ReflectionsData);

    TextureResource mOutputTexture;
    TextureResource mGBufferDepthTexture;
    TextureResource mGbufferRenderTexture;
    DescriptorID mTopLevelAccelerationStructure;
    DescriptorID mInstancesBuffer;
    DescriptorID mMaterialBuffer;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ReflectionsData& AddReflectionsPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene,
    const GBufferData& inGBufferData
);


////////////////////////////////////////
/// Ray-traced Indirect Diffuse Render Pass
////////////////////////////////////////
struct PathTraceData
{
    RTTI_DECLARE_TYPE(PathTraceData);

    uint32_t mBounces = 3;
    TextureResource mOutputTexture;
    TextureResource mGBufferDepthTexture;
    TextureResource mGbufferRenderTexture;
    DescriptorID mTopLevelAccelerationStructure;
    DescriptorID mInstancesBuffer;
    DescriptorID mMaterialBuffer;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const PathTraceData& AddPathTracePass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene,
    const GBufferData& inGBufferData
);


////////////////////////////////////////
/// Downsample Render Pass
////////////////////////////////////////
struct DownsampleData
{
    RTTI_DECLARE_TYPE(DownsampleData);

    TextureID mTextureMips[12];
    BufferID mGlobalAtomicBuffer;
    TextureResource mSourceTexture;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const DownsampleData& AddDownsamplePass(RenderGraph& inRenderGraph, Device& inDevice,
    const TextureResource& inSourceTexture
);


////////////////////////////////////////
/// GI Probe Trace Compute Pass
////////////////////////////////////////
struct ProbeTraceData
{
    RTTI_DECLARE_TYPE(ProbeTraceData);

    ProbeTraceData()
    {
        mDDGIData.mCornerPosition = Vec3(-65, -2, -28.5);
        mDDGIData.mProbeCount = IVec3(22, 22, 22);
        mDDGIData.mProbeSpacing = Vec3(6.4, 2.8, 2.8);
    }

    IVec3           mDebugProbe = IVec3(10, 10, 5);
    DDGIData        mDDGIData;
    Mat3x3          mRandomRotationMatrix;
    DescriptorID    mMaterialBuffer;
    DescriptorID    mInstancesBuffer;
    TextureResource mProbesDepthTexture;
    TextureResource mProbesIrradianceTexture;
    TextureResource mRaysDepthTexture;
    TextureResource mRaysIrradianceTexture;
    DescriptorID    mTopLevelAccelerationStructure;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ProbeTraceData& AddProbeTracePass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene
);


////////////////////////////////////////
/// GI Probe Update Compute Pass
////////////////////////////////////////
struct ProbeUpdateData
{
    RTTI_DECLARE_TYPE(ProbeUpdateData);

    DDGIData        mDDGIData;
    TextureResource mProbesDepthTexture;
    TextureResource mProbesIrradianceTexture;
    TextureResource mRaysDepthTexture;
    TextureResource mRaysIrradianceTexture;
    DescriptorID mTopLevelAccelerationStructure;
    ComPtr<ID3D12PipelineState> mDepthPipeline;
    ComPtr<ID3D12PipelineState> mIrradiancePipeline;
};

const ProbeUpdateData& AddProbeUpdatePass(RenderGraph& inRenderGraph, Device& inDevice,
    const ProbeTraceData& inTraceData
);



////////////////////////////////////////
/// GI Probe Debug Render Pass
////////////////////////////////////////
struct ProbeDebugData
{
    RTTI_DECLARE_TYPE(ProbeDebugData);

    Mesh            mProbeMesh;
    DDGIData        mDDGIData;
    TextureResource mRenderTarget;
    TextureResource mDepthTarget;
    TextureResource mProbesDepthTexture;
    TextureResource mProbesIrradianceTexture;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ProbeDebugData& AddProbeDebugPass(RenderGraph& inRenderGraph, Device& inDevice,
    const ProbeTraceData& inTraceData,
    const ProbeUpdateData& inUpdateData,
    TextureResource inRenderTarget,
    TextureResource inDepthTarget
);


////////////////////////////////////////
/// Debug Lines Render Pass
////////////////////////////////////////
struct DebugLinesData
{
    RTTI_DECLARE_TYPE(DebugLinesData);

    BufferResource mVertexBuffer;
    D3D12_DRAW_ARGUMENTS* mMappedPtr;
    BufferResource mIndirectArgsBuffer;
    ComPtr<ID3D12PipelineState> mPipeline;
    ComPtr<ID3D12CommandSignature> mCommandSignature;
};

const DebugLinesData& AddDebugLinesPass(RenderGraph& inRenderGraph, Device& inDevice,
    TextureResource inRenderTarget,
    TextureResource inDepthTarget
);


////////////////////////////////////////
/// Deferred Lighting Render Pass
////////////////////////////////////////
struct LightingData
{
    RTTI_DECLARE_TYPE(LightingData);

    DDGIData        mDDGIData;
    TextureResource mOutputTexture;
    TextureResource mShadowMaskTexture;
    TextureResource mReflectionsTexture;
    TextureResource mGBufferDepthTexture;
    TextureResource mGBufferRenderTexture;
    TextureResource mAmbientOcclusionTexture;
    TextureResource mProbesDepthTexture;
    TextureResource mProbesIrradianceTexture;
    TextureResource mIndirectDiffuseTexture;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const LightingData& AddLightingPass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferData& inGBufferData,
    const RTShadowMaskData& inShadowMaskData,
    const ReflectionsData& inReflectionsData,
    const RTAOData& inAmbientOcclusionData,
    const ProbeUpdateData& inProbeData
);


////////////////////////////////////////
/// AMD FSR 2.1 Render Pass
////////////////////////////////////////
struct FSR2Data
{
    RTTI_DECLARE_TYPE(FSR2Data);

    TextureResource mColorTexture;
    TextureResource mDepthTexture;
    TextureResource mMotionVectorTexture;
    TextureResource mOutputTexture;
    float mDeltaTime = 0;
    uint32_t mFrameCounter = 0;
};

const FSR2Data& AddFsrPass(RenderGraph& inRenderGraph, Device& inDevice,
    FfxFsr2Context& inContext,
    TextureResource inColorTexture,
    const GBufferData& inGBufferData
);


////////////////////////////////////////
/// Final Compose Render Pass
////////////////////////////////////////
struct ComposeData
{
    RTTI_DECLARE_TYPE(ComposeData);

    TextureResource mInputTexture;
    TextureResource mOutputTexture;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ComposeData& AddComposePass(RenderGraph& inRenderGraph, Device& inDevice,
    TextureResource inInputTexture
);


////////////////////////////////////////
/// Pre-ImGui Pass
////////////////////////////////////////
struct PreImGuiData
{
    RTTI_DECLARE_TYPE(PreImGuiData);

    TextureResource mDisplayTexture;
};

const PreImGuiData& AddPreImGuiPass(RenderGraph& inRenderGraph, Device& inDevice,
    TextureResource ioDisplayTexture
);


////////////////////////////////////////
/// ImGui Render Pass
/// Note: the ImGui init and render functions really don't like being put in the RenderGraph (capturing lambdas) so manual functions are provided.
////////////////////////////////////////
struct ImGuiData
{
    RTTI_DECLARE_TYPE(ImGuiData);

    BufferID mIndexBuffer;
    BufferID mVertexBuffer;
    TextureResource mInputTexture;
    std::vector<uint8_t> mIndexScratchBuffer;
    std::vector<uint8_t> mVertexScratchBuffer;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ImGuiData& AddImGuiPass(RenderGraph& inRenderGraph, Device& inDevice,
    StagingHeap& inStagingHeap,
    TextureResource inInputTexture
);

/* Initializes ImGui and returns the font texture ID. */
[[nodiscard]] TextureID InitImGui(Device& inDevice, DXGI_FORMAT inRtvFormat, uint32_t inFrameCount);

/* Renders ImGui directly to the backbuffer. */
void RenderImGui(RenderGraph& inRenderGraph, Device& inDevice, CommandList& inCmdList);

} // namespace Raekor