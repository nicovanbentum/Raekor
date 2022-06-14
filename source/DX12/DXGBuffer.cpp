#include "pch.h"
#include "DXGBuffer.h"
#include "DXUtil.h"

#include "Raekor/scene.h"
#include "Raekor/camera.h"

namespace Raekor::DX {

void GBufferPass::Init(const Viewport& inViewport, const ShaderLibrary& inShaders, Device& inDevice) {
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
    dsv_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    m_DepthStencil = inDevice.CreateTexture(Texture::Desc{
        .format = DXGI_FORMAT_R24G8_TYPELESS,
        .width  = inViewport.size.x,
        .height = inViewport.size.y,
        .usage  = Texture::DEPTH_STENCIL_TARGET,
        .viewDesc = &dsv_desc
    }, L"RT_GBUFFER_DEPTH24_STENCIL8");

    m_RenderTarget = inDevice.CreateTexture(Texture::Desc{
        .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
        .width  = inViewport.size.x,
        .height = inViewport.size.y,
        .usage  = Texture::RENDER_TARGET,
    }, L"RT_GBUFFER_R32G32B32A32_FLOAT");

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Texture2D.MipLevels = -1;

    m_DepthStencilSRV = inDevice.CreateTextureView(m_DepthStencil, Texture::Desc{
        .usage = Texture::SHADER_SAMPLE,
        .viewDesc = &srv_desc
    });

    m_RenderTargetSRV = inDevice.CreateTextureView(m_RenderTarget, Texture::Desc{
        .usage = Texture::SHADER_SAMPLE,
        .viewDesc = nullptr
    });

    const auto& vertexShader = inShaders.at("gbufferVS");
    const auto& pixelShader  = inShaders.at("gbufferPS");

    constexpr std::array vertex_layout = {
        D3D12_INPUT_ELEMENT_DESC { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psd = {};
    psd.VS = CD3DX12_SHADER_BYTECODE(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
    psd.PS = CD3DX12_SHADER_BYTECODE(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
    psd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psd.InputLayout.NumElements = vertex_layout.size();
    psd.InputLayout.pInputElementDescs = vertex_layout.data();
    psd.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psd.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psd.RasterizerState.FrontCounterClockwise = TRUE;
    psd.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psd.SampleMask = UINT_MAX;
    psd.SampleDesc.Count = 1;
    psd.NumRenderTargets = 1;
    psd.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
    psd.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psd.pRootSignature = inDevice.GetGlobalRootSignature();

    gThrowIfFailed(inDevice->CreateGraphicsPipelineState(&psd, IID_PPV_ARGS(&m_Pipeline)));
}


void GBufferPass::Render(const Viewport& inViewport, const Scene& inScene, const Device& inDevice, ID3D12GraphicsCommandList* inCmdList) {
    inCmdList->SetPipelineState(m_Pipeline.Get());
    inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    auto& rtv_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto& dsv_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    auto& rtv_texture = inDevice.GetTexture(m_RenderTarget);
    auto& dsv_texture = inDevice.GetTexture(m_DepthStencil);

    const auto viewport = CD3DX12_VIEWPORT(rtv_texture.GetResource().Get());
    const auto scissor  = CD3DX12_RECT(0, 0, inViewport.size.x, inViewport.size.y);
    inCmdList->RSSetViewports(1, &viewport);
    inCmdList->RSSetScissorRects(1, &scissor);
    
    auto rtv_cpu_handle = rtv_heap.GetCPUDescriptorHandle(rtv_texture.GetView());
    auto dsv_cpu_handle = dsv_heap.GetCPUDescriptorHandle(dsv_texture.GetView());
    inCmdList->OMSetRenderTargets(1, &rtv_cpu_handle, FALSE, &dsv_cpu_handle);

    const auto clearColour = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    const auto clear_rect = CD3DX12_RECT(0, 0, inViewport.size.x, inViewport.size.y);
    inCmdList->ClearRenderTargetView(rtv_cpu_handle, glm::value_ptr(clearColour), 1, &clear_rect);
    inCmdList->ClearDepthStencilView(dsv_cpu_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 1, &clear_rect);

    const std::array heaps = { inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetHeap() };
    inCmdList->SetDescriptorHeaps(heaps.size(), heaps.data());
    inCmdList->SetGraphicsRootSignature(inDevice.GetGlobalRootSignature());
    
    mRootConstants.mViewProj = inViewport.GetCamera().GetProjection() * inViewport.GetCamera().GetView();
    inCmdList->SetGraphicsRoot32BitConstants(0, sizeof(mRootConstants) / sizeof(DWORD), &mRootConstants, 0);

    for (const auto& [entity, mesh] : inScene.view<Mesh>().each()) {
        const auto& indexBuffer = inDevice.GetBuffer(BufferID(mesh.indexBuffer));
        const auto& vertexBuffer = inDevice.GetBuffer(BufferID(mesh.vertexBuffer));

        D3D12_INDEX_BUFFER_VIEW indexView = {};
        indexView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
        indexView.Format = DXGI_FORMAT_R32_UINT;
        indexView.SizeInBytes = mesh.indices.size() * sizeof(mesh.indices[0]);

        D3D12_VERTEX_BUFFER_VIEW vertexView = {};
        vertexView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
        vertexView.SizeInBytes = vertexBuffer->GetDesc().Width;
        vertexView.StrideInBytes = 44; // TODO: derive from input layout since its all tightly packed

        if (auto material = inScene.try_get<Material>(mesh.material)) {
            mRootConstants.mAlbedo = material->albedo;
            mRootConstants.mProperties.x = material->metallic;
            mRootConstants.mProperties.y = material->roughness;
            mRootConstants.mTextures.x = material->gpuAlbedoMap;
            mRootConstants.mTextures.y = material->gpuNormalMap;
            mRootConstants.mTextures.z = material->gpuMetallicRoughnessMap;
            inCmdList->SetGraphicsRoot32BitConstants(0, offsetof(RootConstants, mViewProj) / sizeof(DWORD), &mRootConstants, 0);
        }

        inCmdList->IASetIndexBuffer(&indexView);
        inCmdList->IASetVertexBuffers(0, 1, &vertexView);

        inCmdList->DrawIndexedInstanced(mesh.indices.size(), 1, 0, 0, 0);
    }
}

} // namespace Raekor::DX