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
    PIXBeginEvent(inColor, inLabel, static_cast<ID3D12GraphicsCommandList*>( *this ));
}


void CommandList::PopMarker()
{
    PIXEndEvent(static_cast<ID3D12GraphicsCommandList*>( *this ));
}


void CommandList::ClearBuffer(Device& inDevice, BufferID inBuffer, Vec4 inValue)
{
    ID3D12Resource* resource_ptr = inDevice.GetD3D12Resource(inBuffer);
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_buffer_handle = inDevice.GetCPUDescriptorHandle(inBuffer);
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_buffer_handle = inDevice.GetGPUDescriptorHandle(inBuffer);

    DescriptorID temp_descriptor = inDevice.GetClearHeap().Add(inDevice.GetBuffer(inBuffer).GetD3D12Resource());
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_temp_descriptor_handle = inDevice.GetClearHeap().GetCPUDescriptorHandle(temp_descriptor);

    inDevice->CopyDescriptorsSimple(1, cpu_temp_descriptor_handle, cpu_buffer_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    m_CommandList->ClearUnorderedAccessViewFloat(gpu_buffer_handle, cpu_temp_descriptor_handle, resource_ptr, glm::value_ptr(inValue), 0, NULL);
}


void CommandList::ClearTexture(Device& inDevice, TextureID inTexture, Vec4 inValue)
{
    ID3D12Resource* resource_ptr = inDevice.GetD3D12Resource(inTexture);
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_buffer_handle = inDevice.GetCPUDescriptorHandle(inTexture);
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_buffer_handle = inDevice.GetGPUDescriptorHandle(inTexture);

    DescriptorHeap& clear_heap = inDevice.GetClearHeap();
    DescriptorID temp_descriptor = clear_heap.Add(inDevice.GetTexture(inTexture).GetD3D12Resource());
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_temp_descriptor_handle = clear_heap.GetCPUDescriptorHandle(temp_descriptor);

    inDevice->CopyDescriptorsSimple(1, cpu_temp_descriptor_handle, cpu_buffer_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    m_CommandList->ClearUnorderedAccessViewFloat(gpu_buffer_handle, cpu_temp_descriptor_handle, resource_ptr, glm::value_ptr(inValue), 0, NULL);
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
        case EBindSlot::CBV0: case EBindSlot::CBV1:
            m_CommandList->SetGraphicsRootConstantBufferView(inSlot, inBuffer->GetGPUVirtualAddress() + inOffset);
            m_CommandList->SetComputeRootConstantBufferView(inSlot, inBuffer->GetGPUVirtualAddress() + inOffset);
            break;
        case EBindSlot::SRV0: case EBindSlot::SRV1:
            m_CommandList->SetGraphicsRootShaderResourceView(inSlot, inBuffer->GetGPUVirtualAddress() + inOffset);
            m_CommandList->SetComputeRootShaderResourceView(inSlot, inBuffer->GetGPUVirtualAddress() + inOffset);
            break;
        default: assert(false);
    }
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

