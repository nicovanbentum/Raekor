#pragma once

#include "RenderGraph.h"

namespace RK::DX12 {

struct GBufferData;

enum EUpscaler
{
    UPSCALER_NONE,
    UPSCALER_FSR,
    UPSCALER_DLSS,
    UPSCALER_XESS,
    UPSCALER_COUNT
};


enum EUpscalerQuality
{
    UPSCALER_QUALITY_NATIVE,
    UPSCALER_QUALITY_QUALITY,
    UPSCALER_QUALITY_BALANCED,
    UPSCALER_QUALITY_PERFORMANCE,
    UPSCALER_QUALITY_COUNT
};

class Upscaler
{
private:
    struct Settings
    {
        int& mUpscaler       = g_CVars.Create("r_upscaler",         0, true);
        int& mUpscaleQuality = g_CVars.Create("r_upscaler_quality", 0, true);
    } m_Settings;

public:
    static UVec2 sGetRenderResolution(UVec2 inDisplayResolution, EUpscalerQuality inQuality);
    static xess_quality_settings_t sGetQualityXeSS(EUpscalerQuality inQuality);
    static NVSDK_NGX_PerfQuality_Value sGetQualityDLSS(EUpscalerQuality inQuality);

    EUpscaler GetActiveUpscaler() const { return EUpscaler(m_Settings.mUpscaler); }
    void SetActiveUpscaler(EUpscaler inUpscaler) { m_Settings.mUpscaler = inUpscaler; }

    EUpscalerQuality GetActiveUpscalerQuality() const { return EUpscalerQuality(m_Settings.mUpscaleQuality); }
    void SetActiveUpscalerQuality(EUpscalerQuality inQuality) { m_Settings.mUpscaleQuality = inQuality; }

    bool InitFSR(Device& inDevice, const Viewport& inViewport);
    bool InitDLSS(Device& inDevice, const Viewport& inViewport, CommandList& inCmdList);
    bool InitXeSS(Device& inDevice, const Viewport& inViewport);
    
    bool DestroyFSR(Device& inDevice);
    bool DestroyDLSS(Device& inDevice);
    bool DestroyXeSS(Device& inDevice);

    FfxFsr2Context* GetFsr2Context() { return &m_Fsr2Context; }
    xess_context_handle_t GetXeSSContext() { return m_XeSSContext; }

    NVSDK_NGX_Handle* GetDLSSHandle() { return m_DLSSHandle; }
    NVSDK_NGX_Parameter* GetDLSSParams() { return m_DLSSParams; }

private:
    FfxFsr2Context          m_Fsr2Context;
    Array<uint8_t>          m_FsrScratchMemory;
    NVSDK_NGX_Handle*       m_DLSSHandle = nullptr;
    NVSDK_NGX_Parameter*    m_DLSSParams = nullptr;
    xess_context_handle_t   m_XeSSContext = nullptr;
};



//////////////////////////////////////////
///// AMD FSR 2.1 Render Pass
//////////////////////////////////////////
struct FSR2Data
{
    RTTI_DECLARE_TYPE(FSR2Data);

    float mDeltaTime = 0;
    uint32_t mFrameCounter = 0;

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mColorTextureSRV;
    RenderGraphResourceViewID mDepthTextureSRV;
    RenderGraphResourceViewID mMotionVectorTextureSRV;
};

const FSR2Data& AddFsrPass(RenderGraph& inRenderGraph, Device& inDevice, Upscaler& inUpscaler,
    RenderGraphResourceID inColorTexture,
    const GBufferData& inGBufferData
);



//////////////////////////////////////////
///// Nvidia DLSS Render Pass
//////////////////////////////////////////
struct DLSSData
{
    RTTI_DECLARE_TYPE(DLSSData);

    uint32_t mFrameCounter = 0;
  
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mColorTextureSRV;
    RenderGraphResourceViewID mDepthTextureSRV;
    RenderGraphResourceViewID mMotionVectorTextureSRV;
};

const DLSSData& AddDLSSPass(RenderGraph& inRenderGraph, Device& inDevice,  Upscaler& inUpscaler,
    RenderGraphResourceID inColorTexture,
    const GBufferData& inGBufferData
);



//////////////////////////////////////////
///// Intel XeSS Render Pass
//////////////////////////////////////////
struct XeSSData
{
    RTTI_DECLARE_TYPE(XeSSData);

    uint32_t mFrameCounter = 0;

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mColorTextureSRV;
    RenderGraphResourceViewID mDepthTextureSRV;
    RenderGraphResourceViewID mMotionVectorTextureSRV;
};

const XeSSData& AddXeSSPass(RenderGraph& inRenderGraph, Device& inDevice, Upscaler& inUpscaler,
    RenderGraphResourceID inColorTexture,
    const GBufferData& inGBufferData
);

} // namespace raekor