#include "pch.h"
#include "DXBlit.h"
#include "DXUtil.h"
#include "DXSampler.h"

namespace Raekor::DX {

void BlitPass::Init(Device& inDevice, const ShaderLibrary& inShaders) {
	const auto& vertexShader = inShaders.at("blitVS.hlsl");
	const auto& pixelShader = inShaders.at("blitPS.hlsl");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psd = {};
    psd.VS = CD3DX12_SHADER_BYTECODE(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
    psd.PS = CD3DX12_SHADER_BYTECODE(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
    psd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psd.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psd.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psd.RasterizerState.FrontCounterClockwise = TRUE;
    psd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psd.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psd.DepthStencilState.DepthEnable = FALSE;
    psd.SampleMask = UINT_MAX;
    psd.SampleDesc.Count = 1;
    psd.NumRenderTargets = 1;
    psd.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    psd.pRootSignature = inDevice.GetGlobalRootSignature();

    gThrowIfFailed(inDevice->CreateGraphicsPipelineState(&psd, IID_PPV_ARGS(&m_Pipeline)));
}


void BlitPass::Render(Device& inDevice, ID3D12GraphicsCommandList* inCmdList, uint32_t inSrc, uint32_t inDst, ESampler inSampler) {
    mPushConstants.mSampleIndex = inSampler;
    mPushConstants.mSourceTextureIndex = inSrc;

    const auto dest_texture = inDevice.m_RtvHeap[inDst];
    const auto render_target = inDevice.m_RtvHeap.GetCPUDescriptorHandle(inDst);
    const std::array heaps = { inDevice.m_CbvSrvUavHeap.GetHeap(), inDevice.m_SamplerHeap.GetHeap() };

    inCmdList->SetPipelineState(m_Pipeline.Get());
    inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    inCmdList->RSSetViewports(1, &CD3DX12_VIEWPORT(dest_texture.Get()));
    inCmdList->RSSetScissorRects(1, &CD3DX12_RECT(0, 0, dest_texture->GetDesc().Width, dest_texture->GetDesc().Height));
    inCmdList->OMSetRenderTargets(1, &render_target, FALSE, nullptr);
    inCmdList->SetGraphicsRootSignature(inDevice.GetGlobalRootSignature());
    inCmdList->SetDescriptorHeaps(heaps.size(), heaps.data());
    inCmdList->SetGraphicsRoot32BitConstants(0, sizeof(mPushConstants) / sizeof(DWORD), &mPushConstants, 0);
    inCmdList->DrawInstanced(6, 1, 0, 0);
}

}