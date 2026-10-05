#include "PCH.h"
#include "CommandList.h"

#include "Camera.h"
#include "Device.h"
#include "Shader.h"
#include "Resource.h"
#include "RenderUtil.h"
#include "Components.h"

namespace RK::DX12 {

CommandList::CommandList(Device& inDevice, D3D12_COMMAND_LIST_TYPE inType) : CommandList(inDevice, inType, 0) {}

CommandList::CommandList(Device& inDevice, D3D12_COMMAND_LIST_TYPE inType, uint32_t inFrameIndex) : m_FrameIndex(inFrameIndex)
{
    gThrowIfFailed(inDevice->CreateCommandAllocator(inType, IID_PPV_ARGS(&m_CommandAllocator)));
    gThrowIfFailed(inDevice->CreateCommandList1(0x00, inType, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_CommandList)));
}


void CommandList::Begin()
{
    gThrowIfFailed(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));
}


void CommandList::Reset()
{
    m_CommandAllocator->Reset();
    gThrowIfFailed(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));
}



void CommandList::Close()
{
    gThrowIfFailed(m_CommandList->Close());
}


void CommandList::PushMarker(const char* inLabel, uint32_t inColor)
{
    PIXBeginEvent(static_cast<ID3D12GraphicsCommandList*>(*this), inColor, inLabel);
}


void CommandList::PopMarker()
{
    PIXEndEvent(static_cast<ID3D12GraphicsCommandList*>( *this ));
}


void CommandList::DiscardTexture(Device& inDevice, TextureID inTexture)
{
#if 0
    ID3D12Resource* resource_ptr = inDevice.GetD3D12Resource(inTexture);
    m_CommandList->DiscardResource(resource_ptr, nullptr);
#endif
}


void CommandList::ClearRenderTarget(Device& inDevice, TextureID inTexture, Vec4 inColorValue)
{
    RK_ASSERT(!gIsDepthFormat(inDevice.GetTexture(inTexture).GetFormat()));

    D3D12_CPU_DESCRIPTOR_HANDLE cpu_Descriptor_handle = inDevice.GetCPUDescriptorHandle(inTexture);
    m_CommandList->ClearRenderTargetView(cpu_Descriptor_handle, &inColorValue[0], 0, nullptr);
}


void CommandList::ClearDepthStencilTarget(Device& inDevice, TextureID inTexture, const float* inDepthValue, const uint8_t* inStencilValue)
{
    RK_ASSERT(gIsDepthFormat(inDevice.GetTexture(inTexture).GetFormat()));

    D3D12_CLEAR_FLAGS clear_flags = D3D12_CLEAR_FLAGS(0);

    float depth_clear_value = 1.0f;
    uint8_t stencil_clear_value = 0u;

    if (inDepthValue)
    {
        depth_clear_value = *inDepthValue;
        clear_flags |= D3D12_CLEAR_FLAG_DEPTH;
    }

    if (inStencilValue)
    {
        stencil_clear_value = *inStencilValue;
        clear_flags |= D3D12_CLEAR_FLAG_STENCIL;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE cpu_Descriptor_handle = inDevice.GetCPUDescriptorHandle(inTexture);
    m_CommandList->ClearDepthStencilView(cpu_Descriptor_handle, clear_flags, depth_clear_value, stencil_clear_value, 0, nullptr);
}


void CommandList::BindDefaults(Device& inDevice)
{
    m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const std::array heaps =
    {
        *inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER),
        *inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    };

    m_CommandList->SetDescriptorHeaps(heaps.size(), heaps.data());
    m_CommandList->SetComputeRootSignature(inDevice.GetGlobalRootSignature());
    m_CommandList->SetGraphicsRootSignature(inDevice.GetGlobalRootSignature());
}


void CommandList::BindToSlot(Buffer& inBuffer, EBindSlot inSlot, uint32_t inOffset)
{
    switch (inSlot)
    {
        case EBindSlot::CBV0: 
        case EBindSlot::CBV1:
            m_CommandList->SetGraphicsRootConstantBufferView(inSlot, inBuffer->GetGPUVirtualAddress() + inOffset);
            m_CommandList->SetComputeRootConstantBufferView(inSlot, inBuffer->GetGPUVirtualAddress() + inOffset);
            break;
        case EBindSlot::SRV0: 
        case EBindSlot::SRV1:
            m_CommandList->SetGraphicsRootShaderResourceView(inSlot, inBuffer->GetGPUVirtualAddress() + inOffset);
            m_CommandList->SetComputeRootShaderResourceView(inSlot, inBuffer->GetGPUVirtualAddress() + inOffset);
            break;
        default: assert(false);
    }
}


void CommandList::BindComputeProgram(const ComputeProgram& inProgram)
{
    m_CommandList->SetPipelineState(inProgram.GetComputePSO());
}


void CommandList::BindGraphicsProgram(const GraphicsProgram& inProgram)
{
    // not implemented yet
    RK_ASSERT(false);
}


void CommandList::Dispatch(uint32_t inThreadGroupCountX, uint32_t inThreadGroupCountY, uint32_t inThreadGroupCountZ)
{
    m_CommandList->Dispatch(inThreadGroupCountX, inThreadGroupCountY, inThreadGroupCountZ);
}


void CommandList::Draw(uint32_t inVertexCount, uint32_t inInstanceCount, int32_t inVertexOffset, uint32_t inInstanceOffset)
{
    m_CommandList->DrawInstanced(inVertexCount, inInstanceCount, inVertexOffset, inInstanceOffset);
}


void CommandList::DrawIndexed(uint32_t inIndexCount, uint32_t inInstanceCount, uint32_t inIndexOffset, int32_t inVertexOffset, uint32_t inInstanceOffset)
{
    m_CommandList->DrawIndexedInstanced(inIndexCount, inInstanceCount, inIndexOffset, inVertexOffset, inInstanceOffset);
}


void CommandList::BindIndexBuffer(Buffer& inBuffer)
{
    const D3D12_INDEX_BUFFER_VIEW index_view =
    {
        .BufferLocation = inBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = inBuffer.GetSize(),
        .Format = inBuffer.GetFormat()
    };

    assert(index_view.SizeInBytes <= inBuffer.GetSize());
    m_CommandList->IASetIndexBuffer(&index_view);
}


void CommandList::BindVertexAndIndexBuffers(Device& inDevice, const RK::Mesh& inMesh)
{
    Buffer& index_buffer = inDevice.GetBuffer(BufferID(inMesh.indexBuffer));
    Buffer& vertex_buffer = inDevice.GetBuffer(BufferID(inMesh.vertexBuffer));

    const D3D12_INDEX_BUFFER_VIEW index_view =
    {
        .BufferLocation = index_buffer->GetGPUVirtualAddress(),
        .SizeInBytes = uint32_t(inMesh.indices.size() * sizeof(inMesh.indices[0])),
        .Format = DXGI_FORMAT_R32_UINT,
    };

    const D3D12_VERTEX_BUFFER_VIEW vertex_view =
    {
        .BufferLocation = vertex_buffer->GetGPUVirtualAddress(),
        .SizeInBytes = uint32_t(vertex_buffer->GetDesc().Width),
        .StrideInBytes = inMesh.GetVertexStride()
    };

    m_CommandList->IASetIndexBuffer(&index_view);
    m_CommandList->IASetVertexBuffers(0, 1, &vertex_view);
}


void CommandList::SetViewportAndScissor(Texture& inTexture, uint32_t inSubresource)
{
    const D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(inTexture.GetD3D12Resource(), inSubresource);
    const D3D12_RECT scissor = CD3DX12_RECT(vp.TopLeftX, vp.TopLeftY, vp.Width, vp.Height);

    m_CommandList->RSSetViewports(1, &vp);
    m_CommandList->RSSetScissorRects(1, &scissor);
}


void CommandList::SetViewportAndScissor(const Viewport& inViewport)
{
    const CD3DX12_RECT scissor = CD3DX12_RECT(0, 0, inViewport.GetRenderSize().x, inViewport.GetRenderSize().y);
    const CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(inViewport.GetRenderSize().x), float(inViewport.GetRenderSize().y));

    m_CommandList->RSSetViewports(1, &viewport);
    m_CommandList->RSSetScissorRects(1, &scissor);
}


void CommandList::Submit(Device& inDevice, ID3D12CommandQueue* inQueue)
{
    assert(m_CommandList->GetType() == inQueue->GetDesc().Type);
    inQueue->ExecuteCommandLists(1, CommandListCast(m_CommandList.GetAddressOf()));
}


} // namespace Raekor

