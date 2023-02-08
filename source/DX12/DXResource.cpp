#include "pch.h"
#include "DXResource.h"

namespace Raekor::DX12 {

D3D12_RESOURCE_STATES gGetResourceStates(Buffer::Usage inUsage) {
	return D3D12_RESOURCE_STATES();
}


D3D12_RESOURCE_STATES gGetResourceStates(Texture::Usage inUsage) {
	switch (inUsage) {
	case Texture::Usage::RENDER_TARGET:
		return D3D12_RESOURCE_STATE_RENDER_TARGET;
	case Texture::Usage::DEPTH_STENCIL_TARGET:
		return D3D12_RESOURCE_STATE_DEPTH_WRITE;
	case Texture::Usage::GENERAL:
		return D3D12_RESOURCE_STATE_COMMON;
	case Texture::Usage::SHADER_READ_ONLY:
		return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
	case Texture::Usage::SHADER_READ_WRITE:
		return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	default:
		assert(false);
	}

	return D3D12_RESOURCE_STATES();
}


D3D12_DESCRIPTOR_HEAP_TYPE gGetHeapType(Buffer::Usage inUsage) {
	assert(false); // TODO: implement
	return D3D12_DESCRIPTOR_HEAP_TYPE();
}


D3D12_DESCRIPTOR_HEAP_TYPE gGetHeapType(Texture::Usage inUsage) {
	switch (inUsage) {
		case Texture::DEPTH_STENCIL_TARGET:
			return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		case Texture::RENDER_TARGET:
			return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		case Texture::SHADER_READ_ONLY:
		case Texture::SHADER_READ_WRITE:
			return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		default:
			assert(false);
	}

	assert(false);
	return D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
}


} // namespace Raekor::DX12