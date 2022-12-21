#include "pch.h"
#include "DXResource.h"

namespace Raekor::DX {

D3D12_RESOURCE_STATES gGetResourceStates(Buffer::Usage inUsage) {
	return D3D12_RESOURCE_STATES();
}


D3D12_RESOURCE_STATES gGetResourceStates(Texture::Usage inUsage) {
	switch (inUsage) {
	case Texture::Usage::RENDER_TARGET:
		return D3D12_RESOURCE_STATE_RENDER_TARGET;
		break;
	case Texture::Usage::DEPTH_STENCIL_TARGET:
		return D3D12_RESOURCE_STATE_DEPTH_WRITE;
		break;
	case Texture::Usage::GENERAL:
		return D3D12_RESOURCE_STATE_COMMON;
		break;
	case Texture::Usage::SHADER_READ_ONLY:
		return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
		break;
	case Texture::Usage::SHADER_READ_WRITE:
		return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		break;
	default:
		assert(false);
	}

	return D3D12_RESOURCE_STATES();
}


} // namespace Raekor::DX