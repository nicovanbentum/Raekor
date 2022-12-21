#pragma once

#include "DXResource.h"
#include "DXRenderGraph.h"

namespace Raekor::DX {

class Device;
class RenderGraph;
class CommandList;


struct BackBufferData {
    uint64_t    mFenceValue;
    TextureID   mBackBuffer;
    CommandList mCmdList;
};

class Renderer {
private:
    struct Settings {
        int& mEnableFsr2 = CVars::sCreate("r_enable_fsr2", 0);
    } m_Settings;

public:
    Renderer(Device& inDevice, const Viewport& inViewport, SDL_Window* inWindow);

    void OnRender(Device& inDevice, float inDeltaTime);
    void OnResize(Device& inDevice, const Viewport& inViewport);

    void WaitForIdle(Device& inDevice);
    void CompileGraph(Device& inDevice, const Scene& inScene, DescriptorID inTLAS);

    CommandList& StartSingleSubmit();
    void FlushSingleSubmit(Device& inDevice, CommandList& inCommandList);

    const RenderGraph&  GetGraph()              { return m_RenderGraph; }
    BackBufferData&     GetBackBufferData()     { return m_BackBufferData[m_FrameIndex];  }
    BackBufferData&     GetPrevBackBufferData() { return m_BackBufferData[!m_FrameIndex]; }

public:
    static constexpr uint32_t sFrameCount = 2;

private:
    uint32_t                m_FrameIndex;
    TextureID               m_ImGuiFontTexture;
    ComPtr<ID3D12Fence>     m_Fence;
    ComPtr<IDXGISwapChain3> m_Swapchain;
    HANDLE                  m_FenceEvent;
    uint64_t                m_FrameCounter = 0;
    BackBufferData          m_BackBufferData[sFrameCount];
    RenderGraph             m_RenderGraph;
    FfxFsr2Context          m_Fsr2Context;
    std::vector<uint8_t>    m_FsrScratchMemory;
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
/// Ray-traced Shadow Mask Render Pass
////////////////////////////////////////
struct ShadowMaskData {
    RTTI_CLASS_HEADER(ShadowMaskData);

    TextureResource mOutputTexture;
    TextureResource mGbufferDepthTexture;
    TextureResource mGBufferRenderTexture;
    DescriptorID mTopLevelAccelerationStructure;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ShadowMaskData& AddShadowMaskPass(RenderGraph& inRenderGraph, Device& inDevice, 
    const Scene& inScene,
    const GBufferData& inGBufferData,
    DescriptorID inTLAS
);


////////////////////////////////////////
/// Deferred Lighting Render Pass
////////////////////////////////////////
struct LightingData {
    RTTI_CLASS_HEADER(LightingData);

    TextureResource mOutputTexture;
    TextureResource mShadowMaskTexture;
    TextureResource mGBufferDepthTexture;
    TextureResource mGBufferRenderTexture;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const LightingData& AddLightingPass(RenderGraph& inRenderGraph, Device& inDevice, 
    const GBufferData& inGBufferData, 
    const ShadowMaskData& inShadowMaskData
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
    FfxFsr2Context& inFsr,
    TextureResource inColorTexture,
    const GBufferData& inGBufferData
);



////////////////////////////////////////
/// ImGui Render Pass
/// Note: the ImGui init and render functions really don't like being put in the RenderGraph (capturing lambdas) so manual functions are provided.
////////////////////////////////////////

/* Initializes ImGui and returns the font texture ID. inRenderGraph must have a valid backbuffer set! */
[[nodiscard]] TextureID InitImGui(RenderGraph& inRenderGraph, Device& inDevice, uint32_t inFrameCount);

/* Renders ImGui directly to the backbuffer. */
void RenderImGui(RenderGraph& inRenderGraph, Device& inDevice, CommandList& inCmdList);

} // namespace Raekor