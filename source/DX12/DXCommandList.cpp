#include "DXCommandList.h"
#include "DXResource.h"
#include "DXDevice.h"

namespace Raekor::DX {

CommandList::CommandList(Device& inDevice) {
	auto& command_list = m_CommandLists.emplace_back();
	auto& command_allocator = m_CommandAllocators.emplace_back();
	gThrowIfFailed(inDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator)));
	gThrowIfFailed(inDevice->CreateCommandList1(0x00, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&command_list)));
}

void CommandList::Begin() {
	auto& cmd_list = m_CommandLists.back();
	gThrowIfFailed(cmd_list->Reset(m_CommandAllocators.back().Get(), nullptr));
}

void CommandList::Reset() {
	auto& cmd_list = m_CommandLists.back();
	m_CommandAllocators.back()->Reset();
	gThrowIfFailed(cmd_list->Reset(m_CommandAllocators.back().Get(), nullptr));
}

void CommandList::Close() {
	auto& cmd_list = m_CommandLists.back();
	gThrowIfFailed(cmd_list->Close());
}

void CommandList::Push() {

}

void CommandList::Pop() {

}


void CommandList::UpdateBuffer(Buffer& inDstBuffer, uint32_t inDstOffset, uint32_t inDstSize, void* inDataPtr) {
}


void CommandList::UpdateTexture(Texture& inDstTexture, uint32_t inDstMip, void* inDataPtr) {
}


void CommandList::Draw() {
}


void CommandList::Dispatch(uint32_t inSizeX, uint32_t inSizeY, uint32_t inSizeZ) {
}


void CommandList::DispatchRays(uint32_t inSizeX, uint32_t inSizeY) {
}


void CommandList::BindToSlot(Buffer& inBuffer, EBindSlot inSlot, uint32_t inOffset) {
	auto& command_list = m_CommandLists[m_CurrentCmdListIndex];

	switch (inSlot) {
	case EBindSlot::CBV0: case EBindSlot::CBV1: case EBindSlot::CBV2: case EBindSlot::CBV3:
		command_list->SetGraphicsRootConstantBufferView(inSlot, inBuffer.GetResource()->GetGPUVirtualAddress() + inOffset);
		break;
	case EBindSlot::SRV0: case EBindSlot::SRV1: case EBindSlot::SRV2: case EBindSlot::SRV3:
		command_list->SetGraphicsRootShaderResourceView(inSlot, inBuffer.GetResource()->GetGPUVirtualAddress() + inOffset);
		break;
	case EBindSlot::UAV0: case EBindSlot::UAV1: case EBindSlot::UAV2: case EBindSlot::UAV3:
		command_list->SetGraphicsRootUnorderedAccessView(inSlot, inBuffer.GetResource()->GetGPUVirtualAddress() + inOffset);
		break;
	default: assert(false);
	}
}


void CommandList::SetViewportScissorRect(const Viewport& inViewport) {
	const auto scissor = CD3DX12_RECT(0, 0, inViewport.size.x, inViewport.size.y);
	const auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(inViewport.size.x), float(inViewport.size.y));

	m_CommandLists[m_CurrentCmdListIndex]->RSSetViewports(1, &viewport);
	m_CommandLists[m_CurrentCmdListIndex]->RSSetScissorRects(1, &scissor);
}


}
