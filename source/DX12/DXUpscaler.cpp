#include "pch.h"
#include "DXUpscaler.h"
#include "DXRenderPasses.h"

namespace Raekor::DX12 {

RTTI_DEFINE_TYPE(FSR2Data) {}
RTTI_DEFINE_TYPE(XeSSData) {}
RTTI_DEFINE_TYPE(DLSSData) {}



UVec2 Upscaler::sGetRenderResolution(UVec2 inDisplayResolution, EUpscalerQuality inQuality)
{
    auto display_res = Vec2(inDisplayResolution);

    switch (inQuality)
    {
        case UPSCALER_QUALITY_QUALITY: display_res /= 1.5f; break;
        case UPSCALER_QUALITY_BALANCED: display_res /= 1.7f; break;
        case UPSCALER_QUALITY_PERFORMANCE: display_res /= 2.0f; break;
        default: break;
    }

    return UVec2(glm::floor(display_res));
}



NVSDK_NGX_PerfQuality_Value Upscaler::sGetQuality(EUpscalerQuality inQuality)
{
    switch (inQuality)
    {
        case UPSCALER_QUALITY_NATIVE: return NVSDK_NGX_PerfQuality_Value_DLAA;
        case UPSCALER_QUALITY_QUALITY: return NVSDK_NGX_PerfQuality_Value_MaxQuality;
        case UPSCALER_QUALITY_BALANCED: return NVSDK_NGX_PerfQuality_Value_Balanced;
        case UPSCALER_QUALITY_PERFORMANCE: return NVSDK_NGX_PerfQuality_Value_MaxPerf;
        default: assert(false);
    }

    gUnreachableCode();
}



bool Upscaler::InitFSR(Device& inDevice, const Viewport& inViewport)
{
    auto fsr2_desc = FfxFsr2ContextDescription
    {
        .flags = FfxFsr2InitializationFlagBits::FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE,
        .maxRenderSize = { inViewport.GetDisplaySize().x, inViewport.GetDisplaySize().y },
        .displaySize = { inViewport.GetDisplaySize().x, inViewport.GetDisplaySize().y },
        .device = ffxGetDeviceDX12(*inDevice),
    };

    m_FsrScratchMemory.resize(ffxFsr2GetScratchMemorySizeDX12());
    auto ffx_error = ffxFsr2GetInterfaceDX12(&fsr2_desc.callbacks, *inDevice, m_FsrScratchMemory.data(), m_FsrScratchMemory.size());
    if (ffx_error != FFX_OK)
        return false;

    ffx_error = ffxFsr2ContextCreate(&m_Fsr2Context, &fsr2_desc);
    if (ffx_error != FFX_OK)
        return false;

    return true;
}



bool Upscaler::DestroyFSR(Device& inDevice)
{
    return ffxFsr2ContextDestroy(&m_Fsr2Context) == FFX_OK;
}



bool Upscaler::InitDLSS(Device& inDevice, const Viewport& inViewport, CommandList& inCmdList)
{
    if (!inDevice.IsDLSSSupported())
        return false;

    if (m_DLSSParams == nullptr)
        gThrowIfFailed(NVSDK_NGX_D3D12_GetCapabilityParameters(&m_DLSSParams));

    uint32_t pOutRenderOptimalWidth;
    uint32_t pOutRenderOptimalHeight;
    uint32_t pOutRenderMaxWidth;
    uint32_t pOutRenderMaxHeight;
    uint32_t pOutRenderMinWidth;
    uint32_t pOutRenderMinHeight;
    float pOutSharpnes;

    const auto upscaler_quality = Upscaler::sGetQuality(EUpscalerQuality(m_Settings.mUpscaleQuality));

    auto result = NGX_DLSS_GET_OPTIMAL_SETTINGS(m_DLSSParams, inViewport.GetDisplaySize().x, inViewport.GetDisplaySize().y,
        upscaler_quality, &pOutRenderOptimalWidth, &pOutRenderOptimalHeight, &pOutRenderMaxWidth, &pOutRenderMaxHeight, &pOutRenderMinWidth, &pOutRenderMinHeight, &pOutSharpnes);

    if (result != NVSDK_NGX_Result_Success)
        return false;

    int DlssCreateFeatureFlags = NVSDK_NGX_DLSS_Feature_Flags_None;
    DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_IsHDR;
    DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
    DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_MVJittered;
    //DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;
    //DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_DoSharpening;
    DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;

    NVSDK_NGX_DLSS_Create_Params DlssCreateParams = {};
    DlssCreateParams.Feature.InWidth = pOutRenderOptimalWidth;
    DlssCreateParams.Feature.InHeight = pOutRenderOptimalHeight;
    DlssCreateParams.Feature.InTargetWidth = inViewport.GetDisplaySize().x;
    DlssCreateParams.Feature.InTargetHeight = inViewport.GetDisplaySize().y;
    DlssCreateParams.Feature.InPerfQualityValue = upscaler_quality;
    DlssCreateParams.InFeatureCreateFlags = DlssCreateFeatureFlags;

    result = NGX_D3D12_CREATE_DLSS_EXT(inCmdList, 1, 1, &m_DLSSHandle, m_DLSSParams, &DlssCreateParams);

    return result;
}



bool Upscaler::DestroyDLSS(Device& inDevice)
{
    if (NVSDK_NGX_D3D12_ReleaseFeature(m_DLSSHandle) != NVSDK_NGX_Result_Success)
        return false;

    m_DLSSHandle = nullptr;

    return true;
}



bool Upscaler::InitXeSS(Device& inDevice, const Viewport& inViewport)
{
    auto status = xessD3D12CreateContext(*inDevice, &m_XeSSContext);
    if (status != XESS_RESULT_SUCCESS)
        return false;

    if (XESS_RESULT_WARNING_OLD_DRIVER == xessIsOptimalDriver(m_XeSSContext))
    {
        SDL_ShowSimpleMessageBox(
            SDL_MessageBoxFlags::SDL_MESSAGEBOX_ERROR, 
            "DX12 Error", "Please install the latest graphics driver from your vendor for optimal Intel(R) XeSS performance and visual quality", NULL);
        return false;
    }

    const auto display_res = xess_2d_t { inViewport.size.x, inViewport.size.y };
    xess_properties_t props;
    status = xessGetProperties(m_XeSSContext, &display_res, &props);
    if (status != XESS_RESULT_SUCCESS)
        return false;

    xess_version_t xefx_version;
    status = xessGetIntelXeFXVersion(m_XeSSContext, &xefx_version);
    if (status != XESS_RESULT_SUCCESS)
        return false;

    const auto uav_desc_size = inDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    xess_d3d12_init_params_t params = {
        /* Output width and height. */
        display_res,
        /* Quality setting */
        XESS_QUALITY_SETTING_ULTRA_QUALITY,
        /* Initialization flags. */
        XESS_INIT_FLAG_ENABLE_AUTOEXPOSURE
    };

    status = xessD3D12Init(m_XeSSContext, &params);
    if (status != XESS_RESULT_SUCCESS)
        return false;

    // Get optimal input resolution
    auto render_res = xess_2d_t {};
    status = xessGetInputResolution(m_XeSSContext, &display_res, XESS_QUALITY_SETTING_ULTRA_QUALITY, &render_res);
    if (status != XESS_RESULT_SUCCESS)
        return false;

    return true;
}



bool Upscaler::DestroyXeSS(Device& inDevice)
{
    return xessDestroyContext(m_XeSSContext) == XESS_RESULT_SUCCESS;
}




const FSR2Data& AddFsrPass(RenderGraph& inRenderGraph, Device& inDevice, Upscaler& inUpscaler, RenderGraphResourceID inColorTexture, const GBufferData& inGBufferData)
{
    return inRenderGraph.AddComputePass<FSR2Data>("FSR2 PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, FSR2Data& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = inRenderGraph.GetViewport().GetDisplaySize().x,
            .height = inRenderGraph.GetViewport().GetDisplaySize().y,
            .usage  = Texture::SHADER_READ_WRITE,
            .debugName = "FSR2_OUTPUT"
        });

        ioRGBuilder.Write(inData.mOutputTexture);

        inData.mColorTextureSRV = ioRGBuilder.Read(inColorTexture);
        inData.mDepthTextureSRV = ioRGBuilder.Read(inGBufferData.mDepthTexture);
        inData.mMotionVectorTextureSRV = ioRGBuilder.Read(inGBufferData.mVelocityTexture);

        inRenderPass->SetExternal(true);
    },

    [&inRenderGraph, &inDevice, &inUpscaler](FSR2Data& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        auto context = inUpscaler.GetFsr2Context();
        auto& viewport = inRenderGraph.GetViewport();

        const auto color_texture_ptr = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mColorTextureSRV));
        const auto depth_texture_ptr = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mDepthTextureSRV));
        const auto movec_texture_ptr = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mMotionVectorTextureSRV));
        const auto output_texture_ptr = inDevice.GetD3D12Resource(inResources.GetTexture(inData.mOutputTexture));

        auto fsr2_dispatch_desc                     = FfxFsr2DispatchDescription {};
        fsr2_dispatch_desc.commandList              = ffxGetCommandListDX12(inCmdList);
        fsr2_dispatch_desc.color                    = ffxGetResourceDX12(context, color_texture_ptr, nullptr, FFX_RESOURCE_STATE_COMPUTE_READ);
        fsr2_dispatch_desc.depth                    = ffxGetResourceDX12(context, depth_texture_ptr, nullptr, FFX_RESOURCE_STATE_COMPUTE_READ);
        fsr2_dispatch_desc.motionVectors            = ffxGetResourceDX12(context, movec_texture_ptr, nullptr, FFX_RESOURCE_STATE_COMPUTE_READ);
        fsr2_dispatch_desc.exposure                 = ffxGetResourceDX12(context, nullptr);
        fsr2_dispatch_desc.output                   = ffxGetResourceDX12(context, output_texture_ptr, nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
        fsr2_dispatch_desc.motionVectorScale.x      = -float(viewport.GetRenderSize().x);
        fsr2_dispatch_desc.motionVectorScale.y      = -float(viewport.GetRenderSize().y);
        fsr2_dispatch_desc.renderSize.width         = viewport.GetRenderSize().x;
        fsr2_dispatch_desc.renderSize.height        = viewport.GetRenderSize().y;
        fsr2_dispatch_desc.enableSharpening         = false;
        fsr2_dispatch_desc.sharpness                = 0.0f;
        fsr2_dispatch_desc.frameTimeDelta           = inData.mDeltaTime * 1000.0f;
        fsr2_dispatch_desc.preExposure              = 1.0f;
        fsr2_dispatch_desc.reset                    = false;
        fsr2_dispatch_desc.cameraNear               = viewport.GetCamera().GetNear();
        fsr2_dispatch_desc.cameraFar                = viewport.GetCamera().GetFar();
        fsr2_dispatch_desc.cameraFovAngleVertical   = glm::radians(viewport.GetFieldOfView());

        const auto jitter_phase_count = ffxFsr2GetJitterPhaseCount(viewport.GetRenderSize().x, viewport.GetDisplaySize().x);
        ffxFsr2GetJitterOffset(&fsr2_dispatch_desc.jitterOffset.x, &fsr2_dispatch_desc.jitterOffset.y, inData.mFrameCounter, jitter_phase_count);
        inData.mFrameCounter = ( inData.mFrameCounter + 1 ) % jitter_phase_count;

        gThrowIfFailed(ffxFsr2ContextDispatch(context, &fsr2_dispatch_desc));
    });
}



const DLSSData& AddDLSSPass(RenderGraph& inRenderGraph, Device& inDevice, Upscaler& inUpscaler, RenderGraphResourceID inColorTexture, const GBufferData& inGBufferData)
{
    return inRenderGraph.AddComputePass<DLSSData>("DLSS PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, DLSSData& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = inRenderGraph.GetViewport().GetDisplaySize().x,
            .height = inRenderGraph.GetViewport().GetDisplaySize().y,
            .usage  = Texture::SHADER_READ_WRITE,
            .debugName = "DLSS_OUTPUT"
        });

        ioRGBuilder.Write(inData.mOutputTexture);

        inData.mColorTextureSRV = ioRGBuilder.Read(inColorTexture);
        inData.mDepthTextureSRV = ioRGBuilder.Read(inGBufferData.mDepthTexture);
        inData.mMotionVectorTextureSRV = ioRGBuilder.Read(inGBufferData.mVelocityTexture);

        inRenderPass->SetExternal(true);
    },

    [&inRenderGraph, &inDevice, &inUpscaler](DLSSData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        auto& viewport = inRenderGraph.GetViewport();

        const auto color_texture_ptr  = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mColorTextureSRV));
        const auto depth_texture_ptr  = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mDepthTextureSRV));
        const auto movec_texture_ptr  = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mMotionVectorTextureSRV));
        const auto output_texture_ptr = inDevice.GetD3D12Resource(inResources.GetTexture(inData.mOutputTexture));

        NVSDK_NGX_D3D12_DLSS_Eval_Params eval_params = {};
        eval_params.Feature.pInColor                 = color_texture_ptr;
        eval_params.Feature.pInOutput                = output_texture_ptr;
        eval_params.pInDepth                         = depth_texture_ptr;
        eval_params.pInMotionVectors                 = movec_texture_ptr;
        eval_params.InMVScaleX                       = -float(viewport.GetRenderSize().x);
        eval_params.InMVScaleY                       = -float(viewport.GetRenderSize().y);
        eval_params.InRenderSubrectDimensions.Width  = viewport.GetRenderSize().x;
        eval_params.InRenderSubrectDimensions.Height = viewport.GetRenderSize().y;

        const auto jitter_phase_count = ffxFsr2GetJitterPhaseCount(viewport.GetRenderSize().x, viewport.GetDisplaySize().x);
        ffxFsr2GetJitterOffset(&eval_params.InJitterOffsetX, &eval_params.InJitterOffsetY, inData.mFrameCounter, jitter_phase_count);
        inData.mFrameCounter = ( inData.mFrameCounter + 1 ) % jitter_phase_count;

        gThrowIfFailed(NGX_D3D12_EVALUATE_DLSS_EXT(inCmdList, inUpscaler.GetDLSSHandle(), inUpscaler.GetDLSSParams(), &eval_params));
    });
}



const XeSSData& AddXeSSPass(RenderGraph& inRenderGraph, Device& inDevice, Upscaler& inUpscaler, RenderGraphResourceID inColorTexture, const GBufferData& inGBufferData)
{
    return inRenderGraph.AddComputePass<XeSSData>("XeSS PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, XeSSData& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = inRenderGraph.GetViewport().GetDisplaySize().x,
            .height = inRenderGraph.GetViewport().GetDisplaySize().y,
            .usage  = Texture::SHADER_READ_WRITE,
            .debugName = "XESS_OUTPUT",
        });

        ioRGBuilder.Write(inData.mOutputTexture);

        inData.mColorTextureSRV = ioRGBuilder.Read(inColorTexture);
        inData.mDepthTextureSRV = ioRGBuilder.Read(inGBufferData.mDepthTexture);
        inData.mMotionVectorTextureSRV = ioRGBuilder.Read(inGBufferData.mVelocityTexture);

        inRenderPass->SetExternal(true);
    },

    [&inRenderGraph, &inDevice, &inUpscaler](XeSSData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        auto& viewport = inRenderGraph.GetViewport();

        const auto color_texture_ptr  = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mColorTextureSRV));
        const auto depth_texture_ptr  = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mDepthTextureSRV));
        const auto movec_texture_ptr  = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mMotionVectorTextureSRV));
        const auto output_texture_ptr = inDevice.GetD3D12Resource(inResources.GetTexture(inData.mOutputTexture));

        xess_d3d12_execute_params_t exec_params = {};
        exec_params.pColorTexture    = color_texture_ptr;
        exec_params.pVelocityTexture = movec_texture_ptr;
        exec_params.pDepthTexture    = depth_texture_ptr;
        exec_params.pOutputTexture   = output_texture_ptr;
        exec_params.exposureScale    = 1.0f;
        exec_params.inputWidth       = viewport.GetRenderSize().x;
        exec_params.inputHeight      = viewport.GetRenderSize().y;

        const auto jitter_phase_count = ffxFsr2GetJitterPhaseCount(viewport.GetRenderSize().x, viewport.GetDisplaySize().x);
        ffxFsr2GetJitterOffset(&exec_params.jitterOffsetX, &exec_params.jitterOffsetY, inData.mFrameCounter, jitter_phase_count);
        inData.mFrameCounter = ( inData.mFrameCounter + 1 ) % jitter_phase_count;

        gThrowIfFailed(xessD3D12Execute(inUpscaler.GetXeSSContext(), inCmdList, &exec_params));
    });
}

} // namespace Raekor::DX12