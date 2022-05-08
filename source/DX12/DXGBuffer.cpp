#include "pch.h"
#include "DXGBuffer.h"
#include "DXUtil.h"

namespace Raekor::DX {

void GBufferPass::Init(const Viewport& inViewport, const ShaderLibrary& inShaders, Device& inDevice) {
    ComPtr<ID3D12Resource> depth_target_res;
    ComPtr<ID3D12Resource> render_target_res;
    
    gThrowIfFailed(inDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R24G8_TYPELESS, inViewport.size.x, inViewport.size.y, 1u, 0, 1u, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, 0u),
        IID_PPV_ARGS(&depth_target_res))
    );
    depth_target_res->SetName(L"RT_GBUFFER_DEPTH24_STENCIL8");

    float clear_colour[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    gThrowIfFailed(inDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, inViewport.size.x, inViewport.size.y, 1u, 0, 1u, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        &CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R32G32B32A32_FLOAT, clear_colour),
        IID_PPV_ARGS(&render_target_res))
    );
    render_target_res->SetName(L"RT_GBUFFER_R32G32B32A32");

    m_DepthStencil = inDevice.m_DsvHeap.AddResource(depth_target_res);
    m_RenderTarget = inDevice.m_RtvHeap.AddResource(render_target_res);
    m_DepthSRV = inDevice.m_CbvSrvUavHeap.AddResource(depth_target_res);
    m_RenderTargetSRV = inDevice.m_CbvSrvUavHeap.AddResource(render_target_res);

    inDevice.CreateRenderTargetView(m_RenderTarget);
    inDevice.CreateShaderResourceView(m_RenderTargetSRV);

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
    dsv_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    inDevice.CreateDepthStencilView(m_DepthStencil, &dsv_desc);
    
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Texture2D.MipLevels = -1;
    
    inDevice.CreateShaderResourceView(m_DepthSRV, &srv_desc);

    const auto& vertexShader = inShaders.at("gbufferVS");
    const auto& pixelShader = inShaders.at("gbufferPS");

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
    psd.RTVFormats[0] = inDevice.m_RtvHeap[m_RenderTarget]->GetDesc().Format;
    psd.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psd.pRootSignature = inDevice.GetGlobalRootSignature();

    gThrowIfFailed(inDevice->CreateGraphicsPipelineState(&psd, IID_PPV_ARGS(&m_Pipeline)));
}


void GBufferPass::Render(const Viewport& inViewport, const Scene& inScene, const Device& inDevice, ID3D12GraphicsCommandList* inCmdList) {
    const auto render_target = inDevice.m_RtvHeap.GetCPUDescriptorHandle(m_RenderTarget);
    const auto depth_stencil_target = inDevice.m_DsvHeap.GetCPUDescriptorHandle(m_DepthStencil);

    inCmdList->SetPipelineState(m_Pipeline.Get());
    inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    inCmdList->RSSetViewports(1, &CD3DX12_VIEWPORT(inDevice.m_RtvHeap[m_RenderTarget].Get()));
    inCmdList->RSSetScissorRects(1, &CD3DX12_RECT(0, 0, inViewport.size.x, inViewport.size.y));
    inCmdList->OMSetRenderTargets(1, &render_target, FALSE, &depth_stencil_target);
    inCmdList->SetGraphicsRootSignature(inDevice.GetGlobalRootSignature());

    const std::array heaps = { inDevice.m_CbvSrvUavHeap.GetHeap() };
    inCmdList->SetDescriptorHeaps(heaps.size(), heaps.data());

    const auto clearColour = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    const auto clear_rect = CD3DX12_RECT(0, 0, inViewport.size.x, inViewport.size.y);
    inCmdList->ClearRenderTargetView(render_target, glm::value_ptr(clearColour), 1, &clear_rect);
    inCmdList->ClearDepthStencilView(depth_stencil_target, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 1, &clear_rect);

    mRootConstants.mViewProj = inViewport.GetCamera().GetProjection() * inViewport.GetCamera().GetView();
    inCmdList->SetGraphicsRoot32BitConstants(0, sizeof(mRootConstants) / sizeof(DWORD), &mRootConstants, 0);

    for (const auto& [entity, mesh] : inScene.view<Mesh>().each()) {
        const auto& indexBuffer = inDevice.m_CbvSrvUavHeap[mesh.indexBuffer];
        const auto& vertexBuffer = inDevice.m_CbvSrvUavHeap[mesh.vertexBuffer];

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