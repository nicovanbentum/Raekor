#include "pch.h"
#include "DXBlit.h"
#include "DXUtil.h"
#include "DXSampler.h"

namespace Raekor::DX {

void PresentPass::Init(Device& inDevice, const ShaderLibrary& inShaders) {
	const auto& vertexShader = inShaders.at("blitVS");
	const auto& pixelShader = inShaders.at("blitPS");

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
    psd.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM; // TODO: kinda hardcoded to the swapchain format, reconsider
    psd.pRootSignature = inDevice.GetGlobalRootSignature();

    gThrowIfFailed(inDevice->CreateGraphicsPipelineState(&psd, IID_PPV_ARGS(&m_Pipeline)));
}


void PresentPass::Render(Device& inDevice, ID3D12GraphicsCommandList* inCmdList, TextureID inSrc, ResourceID inDst, TextureID inGBuffer, ESampler inSampler) {
    const auto& rtv_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    const auto& srv_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    auto& src_texture = inDevice.GetTexture(inSrc);
    auto& gbuffer_texture = inDevice.GetTexture(inGBuffer);
    
    mPushConstants.mSampleIndex = inSampler;
    mPushConstants.mSourceTextureIndex = src_texture.GetHeapIndex();
    mPushConstants.mGbufferTextureIndex = gbuffer_texture.GetHeapIndex();

    const std::array heaps = { 
        inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetHeap(),
        inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).GetHeap(),
    };

    inCmdList->SetPipelineState(m_Pipeline.Get());
    inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const auto render_target = rtv_heap.GetCPUDescriptorHandle(inDst);
    const auto viewport = CD3DX12_VIEWPORT(src_texture.GetResource().Get());
    const auto scissor  = CD3DX12_RECT(0, 0, src_texture->GetDesc().Width, src_texture->GetDesc().Height);
    
    inCmdList->RSSetViewports(1, &viewport);
    inCmdList->RSSetScissorRects(1, &scissor);
    inCmdList->OMSetRenderTargets(1, &render_target, FALSE, nullptr);

    inCmdList->SetDescriptorHeaps(heaps.size(), heaps.data());
    inCmdList->SetGraphicsRootSignature(inDevice.GetGlobalRootSignature());
    inCmdList->SetGraphicsRoot32BitConstants(0, sizeof(mPushConstants) / sizeof(DWORD), &mPushConstants, 0);
    
    inCmdList->DrawInstanced(6, 1, 0, 0);
}

}