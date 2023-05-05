#pragma once

#include "shared.h"
#include "DXResource.h"
#include "DXRenderGraph.h"

namespace Raekor {

class Application;

}

namespace Raekor::DX12 {

class Device;
class RenderGraph;
class CommandList;


struct BackBufferData {
    uint64_t    mFenceValue;
    TextureID   mBackBuffer;
    CommandList mCmdList;
};

/*
    Fun TODO's:
    - timestamp queries per render pass for profiling
    - debug UI to reroute any intermediate texture to screen
*/
class Renderer {
private:
    struct Settings {
        int& mDebugLines  = CVars::sCreate("r_debug_lines", 1);
        int& mProbeDebug  = CVars::sCreate("r_debug_gi_probes", 1);
        int& mEnableRTAO  = CVars::sCreate("r_enable_rtao", 1);
        int& mEnableFsr2  = CVars::sCreate("r_enable_fsr2", 0);
        int& mEnableDDGI  = CVars::sCreate("r_enable_ddgi", 1);
        int& mEnableVsync = CVars::sCreate("r_vsync", 1);
    } m_Settings;

public:
    Renderer(Device& inDevice, const Viewport& inViewport, SDL_Window* inWindow);

    void OnResize(Device& inDevice, const Viewport& inViewport, bool inExclusiveFullscreen = false);
    void OnRender(Device& inDevice, const Viewport& inViewport, Scene& inScene, DescriptorID inTLAS, DescriptorID inInstancesBuffer, DescriptorID inMaterialsBuffer,  float inDeltaTime);

    void Recompile(Device& inDevice, const Scene& inScene, DescriptorID inTLAS, DescriptorID inInstancesBuffer, DescriptorID inMaterialsBuffer);

    CommandList& StartSingleSubmit();
    void FlushSingleSubmit(Device& inDevice, CommandList& inCommandList);
    
    void WaitForIdle(Device& inDevice);

    const RenderGraph&  GetRenderGraph()        { return m_RenderGraph; }
    BackBufferData&     GetBackBufferData()     { return m_BackBufferData[m_FrameIndex];  }
    BackBufferData&     GetPrevBackBufferData() { return m_BackBufferData[!m_FrameIndex]; }

public:
    static constexpr uint32_t sFrameCount = 2;
    static constexpr DXGI_FORMAT sSwapchainFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

private:
    Async::JobPtr           m_PresentJobPtr;
    uint32_t                m_FrameIndex;
    float                   m_ElapsedTime = 0;
    ComPtr<IDXGISwapChain3> m_Swapchain;
    ComPtr<ID3D12Fence>     m_Fence;
    HANDLE                  m_FenceEvent;
    uint64_t                m_FrameCounter = 0;
    bool                    m_ShouldCaptureNextFrame = false;
    BackBufferData          m_BackBufferData[sFrameCount];
    FrameConstants          m_FrameConstants;
    FfxFsr2Context          m_Fsr2Context;
    std::vector<uint8_t>    m_FsrScratchMemory;
    RenderGraph             m_RenderGraph;
};

////////////////////////////////////////
/// GBuffer Render Pass
////////////////////////////////////////
struct GBufferData {
    RTTI_CLASS_HEADER(GBufferData);

    TextureResource mDepthTexture;
    TextureResource mRenderTexture;
    TextureResource mMotionVectorTexture;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const GBufferData& AddGBufferPass(RenderGraph& inRenderGraph, Device& inDevice, 
    const Scene& inScene
);


////////////////////////////////////////
/// Grass Pass
////////////////////////////////////////
struct GrassData {
    RTTI_CLASS_HEADER(GrassData);

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
struct RTShadowMaskData {
    RTTI_CLASS_HEADER(RTShadowMaskData);

    TextureResource mOutputTexture;
    TextureResource mGbufferDepthTexture;
    TextureResource mGBufferRenderTexture;
    DescriptorID mTopLevelAccelerationStructure;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const RTShadowMaskData& AddShadowMaskPass(RenderGraph& inRenderGraph, Device& inDevice, 
    const GBufferData& inGBufferData,
    DescriptorID inTLAS
);


////////////////////////////////////////
/// Ray-traced Shadow Mask Render Pass
////////////////////////////////////////
struct RTAOData {
    RTTI_CLASS_HEADER(RTAOData);

    AmbientOcclusionParams mParams = { .mRadius = 2.0, .mIntensity = 1.0, .mNormalBias = 0.01, .mSampleCount = 1u };
    TextureResource mOutputTexture;
    TextureResource mGbufferDepthTexture;
    TextureResource mGBufferRenderTexture;
    DescriptorID mTopLevelAccelerationStructure;    
    ComPtr<ID3D12PipelineState> mPipeline;
};

const RTAOData& AddAmbientOcclusionPass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferData& inGBufferData,
    DescriptorID inTLAS
);


////////////////////////////////////////
/// Ray-traced Reflections Render Pass
////////////////////////////////////////
struct ReflectionsData {
    RTTI_CLASS_HEADER(ReflectionsData);

    TextureResource mOutputTexture;
    TextureResource mGBufferDepthTexture;
    TextureResource mGbufferRenderTexture;
    DescriptorID mTopLevelAccelerationStructure;
    DescriptorID mInstancesBuffer;
    DescriptorID mMaterialBuffer;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ReflectionsData& AddReflectionsPass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferData& inGBufferData,
    DescriptorID inTLAS,
    DescriptorID inInstancesBuffer,
    DescriptorID inMaterialsBuffer
);


////////////////////////////////////////
/// Ray-traced Indirect Diffuse Render Pass
////////////////////////////////////////
struct IndirectDiffuseData {
    RTTI_CLASS_HEADER(IndirectDiffuseData);

    TextureResource mOutputTexture;
    TextureResource mGBufferDepthTexture;
    TextureResource mGbufferRenderTexture;
    DescriptorID mTopLevelAccelerationStructure;
    DescriptorID mInstancesBuffer;
    DescriptorID mMaterialBuffer;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const IndirectDiffuseData& AddIndirectDiffusePass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferData& inGBufferData,
    DescriptorID inTLAS,
    DescriptorID inInstancesBuffer,
    DescriptorID inMaterialsBuffer
);


////////////////////////////////////////
/// Downsample Render Pass
////////////////////////////////////////
struct DownsampleData {
    RTTI_CLASS_HEADER(DownsampleData);

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
struct ProbeTraceData {
    RTTI_CLASS_HEADER(ProbeTraceData);

    ProbeTraceData() {
        mDDGIData.mCornerPosition = Vec3(-65, -2, -28.5);
        mDDGIData.mProbeCount     = IVec3(22, 22, 22);
        mDDGIData.mProbeSpacing   = Vec3(6.4, 2.8, 2.8);
    }

    IVec3           mDebugProbe = IVec3(10, 10, 5);
    DDGIData        mDDGIData;
    DescriptorID    mMaterialBuffer;
    DescriptorID    mInstancesBuffer;
    TextureResource mProbesDepthTexture;
    TextureResource mProbesIrradianceTexture;
    TextureResource mRaysDepthTexture;
    TextureResource mRaysIrradianceTexture;
    DescriptorID    mTopLevelAccelerationStructure;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ProbeTraceData& AddProbeTracePass(RenderGraph& inRenderGraph, Device& inDevice, DescriptorID inTLAS, DescriptorID inInstancesBuffer, DescriptorID inMaterialsBuffer);


////////////////////////////////////////
/// GI Probe Update Compute Pass
////////////////////////////////////////
struct ProbeUpdateData {
    RTTI_CLASS_HEADER(ProbeUpdateData);

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
struct ProbeDebugData {
    RTTI_CLASS_HEADER(ProbeDebugData);

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
struct DebugLinesData {
    RTTI_CLASS_HEADER(DebugLinesData);

    BufferResource mVertexBuffer;
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
struct LightingData {
    RTTI_CLASS_HEADER(LightingData);

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
/// Final Compose Render Pass
////////////////////////////////////////
struct ComposeData {
    RTTI_CLASS_HEADER(ComposeData);

    TextureResource mInputTexture;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ComposeData& AddComposePass(RenderGraph& inRenderGraph, Device& inDevice, 
    TextureResource inInputTexture
);


////////////////////////////////////////
/// AMD FSR 2.1 Render Pass
////////////////////////////////////////
struct FSR2Data {
    RTTI_CLASS_HEADER(FSR2Data);

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
/// ImGui Render Pass
/// Note: the ImGui init and render functions really don't like being put in the RenderGraph (capturing lambdas) so manual functions are provided.
////////////////////////////////////////

/* Initializes ImGui and returns the font texture ID. */
[[nodiscard]] TextureID InitImGui(Device& inDevice, DXGI_FORMAT inRtvFormat, uint32_t inFrameCount);

/* Renders ImGui directly to the backbuffer. */
void RenderImGui(RenderGraph& inRenderGraph, Device& inDevice, CommandList& inCmdList);

} // namespace Raekor