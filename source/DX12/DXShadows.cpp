#include "pch.h"
#include "DXShadows.h"
#include "DXUtil.h"

namespace Raekor::DX {
	
void ShadowPass::Init(const Viewport& inViewport, const ShaderLibrary& inShaders, Device& inDevice) {
    m_ResultTexture = inDevice.CreateTexture(Texture::Desc{
        .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
        .width  = inViewport.size.x,
        .height = inViewport.size.y,
        .usage  = Texture::Usage::SHADER_WRITE
    }, L"SHADOW_MASK");

    const auto& compute_shader = inShaders.at("shadowsCS");

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.CS = CD3DX12_SHADER_BYTECODE(compute_shader->GetBufferPointer(), compute_shader->GetBufferSize());
    desc.pRootSignature = inDevice.GetGlobalRootSignature();

    gThrowIfFailed(inDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(m_Pipeline.GetAddressOf())));
}


void ShadowPass::Render(const Viewport& inViewport, const Device& inDevice, const Scene& inScene, ResourceID inTLAS, TextureID inGBufferSRV, TextureID inGBufferDepth, ID3D12GraphicsCommandList* inCmdList) {
    inCmdList->SetPipelineState(m_Pipeline.Get());
    inCmdList->SetComputeRootSignature(inDevice.GetGlobalRootSignature());

    const std::array heaps = { inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetHeap() };
    inCmdList->SetDescriptorHeaps(heaps.size(), heaps.data());

    mRootConstants.mTextures.x = inDevice.GetTexture(inGBufferSRV).GetView().ToIndex();
    mRootConstants.mTextures.y = inDevice.GetTexture(inGBufferDepth).GetView().ToIndex();
    mRootConstants.mTextures.z = inDevice.GetTexture(m_ResultTexture).GetView().ToIndex();
    mRootConstants.mTextures.w = inTLAS.ToIndex();

    mRootConstants.invViewProj = glm::inverse(inViewport.GetCamera().GetProjection() * inViewport.GetCamera().GetView());
    mRootConstants.mDispatchSize = inViewport.size;
    mRootConstants.mFrameCounter++;

    auto lightView = inScene.view<const DirectionalLight, const Transform>();
    auto lookDirection = glm::vec3(0.25f, -0.9f, 0.0f);

    // TODO: BUGGED?
    if (lightView.begin() != lightView.end()) {
        const auto& lightTransform = lightView.get<const Transform>(lightView.front());
        lookDirection = static_cast<glm::quat>(lightTransform.rotation) * lookDirection;
    }
    else {
        // we rotate default light a little or else we get nan values in our view matrix
        lookDirection = static_cast<glm::quat>(glm::vec3(glm::radians(15.0f), 0, 0)) * lookDirection;
    }

    lookDirection = glm::clamp(lookDirection, { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f });

    mRootConstants.mLightDir = glm::vec4(lookDirection, 0);

    inCmdList->SetComputeRoot32BitConstants(0, sizeof(mRootConstants) / sizeof(DWORD), &mRootConstants, 0);
    inCmdList->Dispatch(inViewport.size.x / 8, inViewport.size.y / 8, 1);
}

}