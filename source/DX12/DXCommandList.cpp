#include "DXCommandList.h"
#include "DXResource.h"
#include "DXDevice.h"

namespace Raekor::DX12 {

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



void CommandList::Push()
{
}



void CommandList::Pop()
{
}



void CommandList::UpdateBuffer(Buffer& inDstBuffer, uint32_t inDstOffset, uint32_t inDstSize, void* inDataPtr)
{
}



void CommandList::UpdateTexture(Texture& inDstTexture, uint32_t inDstMip, void* inDataPtr)
{
}



void CommandList::Draw()
{
}



void CommandList::Dispatch(uint32_t inSizeX, uint32_t inSizeY, uint32_t inSizeZ)
{
}



void CommandList::DispatchRays(uint32_t inSizeX, uint32_t inSizeY)
{
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



void CommandList::BindVertexAndIndexBuffers(Device& inDevice, const Mesh& inMesh)
{
    const auto& index_buffer = inDevice.GetBuffer(BufferID(inMesh.indexBuffer));
    const auto& vertex_buffer = inDevice.GetBuffer(BufferID(inMesh.vertexBuffer));

    const auto index_view = D3D12_INDEX_BUFFER_VIEW
    {
        .BufferLocation = index_buffer->GetGPUVirtualAddress(),
        .SizeInBytes = uint32_t(inMesh.indices.size() * sizeof(inMesh.indices[0])),
        .Format = DXGI_FORMAT_R32_UINT,
    };

    const auto vertex_view = D3D12_VERTEX_BUFFER_VIEW
    {
        .BufferLocation = vertex_buffer->GetGPUVirtualAddress(),
        .SizeInBytes = uint32_t(vertex_buffer->GetDesc().Width),
        .StrideInBytes = inMesh.GetInterleavedStride()
    };

    m_CommandLists[m_CurrentCmdListIndex]->IASetIndexBuffer(&index_view);
    m_CommandLists[m_CurrentCmdListIndex]->IASetVertexBuffers(0, 1, &vertex_view);
}



void CommandList::SetViewportAndScissor(const Viewport& inViewport)
{
    const auto scissor = CD3DX12_RECT(0, 0, inViewport.GetRenderSize().x, inViewport.GetRenderSize().y);
    const auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(inViewport.GetRenderSize().x), float(inViewport.GetRenderSize().y));

    m_CommandLists[m_CurrentCmdListIndex]->RSSetViewports(1, &viewport);
    m_CommandLists[m_CurrentCmdListIndex]->RSSetScissorRects(1, &scissor);
}


void CommandList::Submit(Device& inDevice, ID3D12CommandQueue* inQueue)
{
    auto& command_list = m_CommandLists[m_CurrentCmdListIndex];
    assert(command_list->GetType() == inQueue->GetDesc().Type);

    const auto cmd_lists = std::array { static_cast<ID3D12CommandList*>( command_list.Get() )};
    inQueue->ExecuteCommandLists(cmd_lists.size(), cmd_lists.data());

    m_SubmitFenceValue++;
}

} // namespace Raekor

