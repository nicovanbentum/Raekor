#include "pch.h"
#include "DXShadows.h"
#include "DXUtil.h"

namespace Raekor::DX {
	
void ShadowPass::Init(const Viewport& inViewport, const ShaderLibrary& inShaders, Device& inDevice) {
    ComPtr<ID3D12Resource> result_texture;
    gThrowIfFailed(inDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, inViewport.size.x, inViewport.size.y, 1u, 0, 1u, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&result_texture))
    );

    result_texture->SetName(L"RT_SHADOWS");
    m_ResultTexture = inDevice.m_CbvSrvUavHeap.AddResource(result_texture);
    inDevice.CreateShaderResourceView(m_ResultTexture);

    const auto& compute_shader = inShaders.at("shadowsCS");

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.CS = CD3DX12_SHADER_BYTECODE(compute_shader->GetBufferPointer(), compute_shader->GetBufferSize());
    desc.pRootSignature = inDevice.GetGlobalRootSignature();

    gThrowIfFailed(inDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(m_Pipeline.GetAddressOf())));
}


void ShadowPass::Render(const Viewport& inViewport, const Device& inDevice, const Scene& inScene, uint32_t inTLAS, uint32_t inGBufferSRV, uint32_t inGBufferDepth, ID3D12GraphicsCommandList* inCmdList) {
    inCmdList->SetPipelineState(m_Pipeline.Get());
    inCmdList->SetComputeRootSignature(inDevice.GetGlobalRootSignature());

    const std::array heaps = { inDevice.m_CbvSrvUavHeap.GetHeap() };
    inCmdList->SetDescriptorHeaps(heaps.size(), heaps.data());

    mRootConstants.mTextures.x = inGBufferSRV;
    mRootConstants.mTextures.y = inGBufferDepth;
    mRootConstants.mTextures.z = m_ResultTexture;
    mRootConstants.mTextures.w = inTLAS;
    mRootConstants.invViewProj = glm::inverse(inViewport.GetCamera().GetProjection() * inViewport.GetCamera().GetView());
    mRootConstants.mDispatchSize = inViewport.size;
    mRootConstants.mFrameCounter++;

    auto lightView = inScene.view<const DirectionalLight, const Transform>();
    auto lookDirection = glm::vec3(0.25f, -0.9f, 0.0f);

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