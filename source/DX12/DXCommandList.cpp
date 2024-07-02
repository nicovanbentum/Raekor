#include "pch.h"
#include "DXCommandList.h"

#include "DXResource.h"
#include "DXDevice.h"
#include "DXShader.h"

namespace RK::DX12 {

CommandList::CommandList(Device& inDevice, D3D12_COMMAND_LIST_TYPE inType) : CommandList(inDevice, inType, 0) {}

CommandList::CommandList(Device& inDevice, D3D12_COMMAND_LIST_TYPE inType, uint32_t inFrameIndex) : m_FrameIndex(inFrameIndex)
{
    auto& command_list = m_CommandLists.emplace_back();
    auto& command_allocator = m_CommandAllocators.emplace_back();
    gThrowIfFailed(inDevice->CreateCommandAllocator(inType, IID_PPV_ARGS(&command_allocator)));
    gThrowIfFailed(inDevice->CreateCommandList1(0x00, inType, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&command_list)));
}


void CommandList::Begin()
{
    auto& cmd_list = m_CommandLists.back();
    gThrowIfFailed(cmd_list->Reset(m_CommandAllocators.back().Get(), nullptr));
}


void CommandList::Reset()
{
    auto& cmd_list = m_CommandLists.back();
    m_CommandAllocators.back()->Reset();
    gThrowIfFailed(cmd_list->Reset(m_CommandAllocators.back().Get(), nullptr));
}



void CommandList::Close()
{
    auto& cmd_list = m_CommandLists.back();
    gThrowIfFailed(cmd_list->Close());
}


void CommandList::ClearBuffer(Device& inDevice, BufferID inBuffer, Vec4 inValue)
{
    auto& cmd_list = m_CommandLists.back();

    ID3D12Resource* resource_ptr = inDevice.GetD3D12Resource(inBuffer);
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_buffer_handle = inDevice.GetCPUDescriptorHandle(inBuffer);
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_buffer_handle = inDevice.GetGPUDescriptorHandle(inBuffer);

    DescriptorID temp_descriptor = inDevice.GetClearHeap().Add(inDevice.GetBuffer(inBuffer).GetD3D12Resource());
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_temp_descriptor_handle = inDevice.GetClearHeap().GetCPUDescriptorHandle(temp_descriptor);

    inDevice->CopyDescriptorsSimple(1, cpu_temp_descriptor_handle, cpu_buffer_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    cmd_list->ClearUnorderedAccessViewFloat(gpu_buffer_handle, cpu_temp_descriptor_handle, resource_ptr, glm::value_ptr(inValue), 0, NULL);
}


void CommandList::ClearTexture(Device& inDevice, TextureID inTexture, Vec4 inValue)
{
    auto& cmd_list = m_CommandLists.back();

    ID3D12Resource* resource_ptr = inDevice.GetD3D12Resource(inTexture);
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_buffer_handle = inDevice.GetCPUDescriptorHandle(inTexture);
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_buffer_handle = inDevice.GetGPUDescriptorHandle(inTexture);

    DescriptorHeap& clear_heap = inDevice.GetClearHeap();
    DescriptorID temp_descriptor = clear_heap.Add(inDevice.GetTexture(inTexture).GetD3D12Resource());
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_temp_descriptor_handle = clear_heap.GetCPUDescriptorHandle(temp_descriptor);

    inDevice->CopyDescriptorsSimple(1, cpu_temp_descriptor_handle, cpu_buffer_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    cmd_list->ClearUnorderedAccessViewFloat(gpu_buffer_handle, cpu_temp_descriptor_handle, resource_ptr, glm::value_ptr(inValue), 0, NULL);
}


void CommandList::BindToSlot(Buffer& inBuffer, EBindSlot inSlot, uint32_t inOffset)
{
    auto& command_list = m_CommandLists[m_CurrentCmdListIndex];

    switch (inSlot)
    {
        case EBindSlot::CBV0:
            command_list->SetGraphicsRootConstantBufferView(inSlot, inBuffer->GetGPUVirtualAddress());
            break;
        case EBindSlot::SRV0: case EBindSlot::SRV1:
            command_list->SetGraphicsRootShaderResourceView(inSlot, inBuffer->GetGPUVirtualAddress() + inOffset);
            command_list->SetComputeRootShaderResourceView(inSlot, inBuffer->GetGPUVirtualAddress() + inOffset);
            break;
        default: assert(false);
    }

    TrackResource(inBuffer);
}


void CommandList::BindIndexBuffer(const Buffer& inBuffer)
{
    const D3D12_INDEX_BUFFER_VIEW index_view =
    {
        .BufferLocation = inBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = inBuffer.GetSize(),
        .Format = inBuffer.GetFormat()
    };

    assert(index_view.SizeInBytes <= inBuffer.GetSize());
    m_CommandLists[m_CurrentCmdListIndex]->IASetIndexBuffer(&index_view);
}


void CommandList::BindVertexAndIndexBuffers(Device& inDevice, const Mesh& inMesh)
{
    const Buffer& index_buffer = inDevice.GetBuffer(BufferID(inMesh.indexBuffer));
    const Buffer& vertex_buffer = inDevice.GetBuffer(BufferID(inMesh.vertexBuffer));

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

    m_CommandLists[m_CurrentCmdListIndex]->IASetIndexBuffer(&index_view);
    m_CommandLists[m_CurrentCmdListIndex]->IASetVertexBuffers(0, 1, &vertex_view);
}



void CommandList::SetViewportAndScissor(const Viewport& inViewport)
{
    const CD3DX12_RECT scissor = CD3DX12_RECT(0, 0, inViewport.GetRenderSize().x, inViewport.GetRenderSize().y);
    const CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(inViewport.GetRenderSize().x), float(inViewport.GetRenderSize().y));

    m_CommandLists[m_CurrentCmdListIndex]->RSSetViewports(1, &viewport);
    m_CommandLists[m_CurrentCmdListIndex]->RSSetScissorRects(1, &scissor);
}


void CommandList::Submit(Device& inDevice, ID3D12CommandQueue* inQueue)
{
    auto& command_list = m_CommandLists[m_CurrentCmdListIndex];
    assert(command_list->GetType() == inQueue->GetDesc().Type);

    const std::array cmd_lists = { static_cast<ID3D12CommandList*>( command_list.Get() )};
    inQueue->ExecuteCommandLists(cmd_lists.size(), cmd_lists.data());

    m_SubmitFenceValue++;
}

} // namespace Raekor

