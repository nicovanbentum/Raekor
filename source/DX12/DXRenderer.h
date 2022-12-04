#pragma once

#include "DXResource.h"

namespace Raekor::DX {

class Device;
class RenderGraph;


////////////////////////////////////////
/// GBuffer Render Pass
////////////////////////////////////////
struct GBufferData {
    RTTI_CLASS_HEADER(GBufferData);

    TextureID mDepthTexture;
    TextureID mRenderTexture;
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

    TextureID mOutputTexture;
    TextureID mGbufferDepthTexture;
    TextureID mGBufferRenderTexture;
    ResourceID mTopLevelAccelerationStructure;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ShadowMaskData& AddShadowMaskPass(RenderGraph& inRenderGraph, Device& inDevice, 
    const Scene& inScene,
    const GBufferData& inGBufferData,
    ResourceID inTLAS
);


////////////////////////////////////////
/// Deferred Lighting Render Pass
////////////////////////////////////////
struct LightingData {
    RTTI_CLASS_HEADER(LightingData);

    TextureID mOutputTexture;
    TextureID mShadowMaskTexture;
    TextureID mGBufferDepthTexture;
    TextureID mGBufferRenderTexture;
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

    TextureID mInputTexture;
    TextureID mOutputTexture;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ComposeData& AddComposePass(RenderGraph& inRenderGraph, Device& inDevice, 
    const GBufferData& inGBufferData, 
    TextureID inBackBuffer
);


}