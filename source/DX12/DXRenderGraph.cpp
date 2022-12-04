#include "pch.h"
#include "DXRenderGraph.h"

namespace Raekor::DX {


bool IRenderPass::IsRead(TextureID inTexture) {
	for (const auto& texture : m_ReadTextures)
		if (texture == inTexture)
			return true;

	return false;
}

bool IRenderPass::IsWritten(TextureID inTexture) {
	for (const auto& texture : m_WrittenTextures)
		if (texture == inTexture)
			return true;

	return false;
}

void IRenderPass::Replace(TextureID inOldTexture, TextureID inNewTexture) {
	for (auto& texture : m_CreatedTextures)
		if (texture == inOldTexture)
			texture = inNewTexture;

	for (auto& texture : m_WrittenTextures)
		if (texture == inOldTexture)
			texture = inNewTexture;

	for (auto& texture : m_ReadTextures)
		if (texture == inOldTexture)
			texture = inNewTexture;
}

bool IRenderPass::IsCreated(TextureID inTexture) {
	for (const auto& texture : m_CreatedTextures)
		if (texture == inTexture)
			return true;

	return false;
}


void RenderGraph::Compile(Device& inDevice) {
	for (const auto& renderpass : m_RenderPasses)
		renderpass->Setup(inDevice);
}
	


void IRenderPass::FlushBarriers(Device& inDevice, CommandList& inCmdList) const {
	auto barriers = std::vector<D3D12_RESOURCE_BARRIER>(m_ResourceBarriers.size());
	
	for (const auto& [index, barrier] : gEnumerate(m_ResourceBarriers))
		barriers[index] = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetResourcePtr(barrier.mTexture), barrier.mBeforeState, barrier.mAfterState);

	if(!barriers.empty())
		inCmdList->ResourceBarrier(barriers.size(), barriers.data());
}



void IRenderPass::SetRenderTargets(Device& inDevice, CommandList& inCmdList) const {
	auto& rtv_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	auto& dsv_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	ResourceID dsv_view; // ResourceID default constructs to invalid, so its invalid until a depth stencil target is found and assigned
	uint32_t rtv_handle_count = 0;
	static D3D12_CPU_DESCRIPTOR_HANDLE rtv_handles[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];

	for (const auto texture_id : m_WrittenTextures) {
		const auto& texture = inDevice.GetTexture(texture_id);

		switch (texture.GetDesc().usage) {
			case Texture::RENDER_TARGET: {
				rtv_handles[rtv_handle_count++] = rtv_heap.GetCPUDescriptorHandle(texture.GetView());
			} break;
			case Texture::DEPTH_STENCIL_TARGET: {
				assert(!dsv_view.Isvalid()); // if you define multiple depth targets for a renderpass you're going to have a bad time, mmkay
				dsv_view = texture.GetView();
			} break;
		}
	}

	if (dsv_view.Isvalid()) {
		auto dsv_handle = dsv_heap.GetCPUDescriptorHandle(dsv_view);
		inCmdList->OMSetRenderTargets(rtv_handle_count, rtv_handles, FALSE, &dsv_handle);
	}
	else
		inCmdList->OMSetRenderTargets(rtv_handle_count, rtv_handles, FALSE, nullptr);
}



	
void RenderGraph::Execute(Device& inDevice, CommandList& inCmdList) {
	inDevice.BindDrawDefaults(inCmdList);

	for (const auto& renderpass : m_RenderPasses) {
		inCmdList.BeginPixEvent(PIX_COLOR(0, 255, 0), renderpass->GetName().c_str());

		renderpass->FlushBarriers(inDevice, inCmdList);

		if (renderpass->IsGraphics())
			renderpass->SetRenderTargets(inDevice, inCmdList);

		renderpass->Execute(inCmdList);
		
		inCmdList.EndPixEvent();
	}
}


} // raekor

