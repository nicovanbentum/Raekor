#pragma once

#include "DXDevice.h"

namespace Raekor {
	class Scene;
	class Viewport;
}

namespace Raekor::DX {

enum ERenderImage {
	RT_GBUFFER_PACKED,
	RT_GBUFFER_DEPTH,
	RT_SHADOW_MASK,
	DEFERRED_LIGHTING,
	NUM_IMAGES
};

class RenderContext {
public:
	RenderContext(Device& inDevice) 
		: m_Device(inDevice) {}

	~RenderContext() = default;

	void CreateImages(Viewport& viewport);
	void DestroyImages();

	auto GetRenderRTID(ERenderImage inImage) { return m_RenderImages[inImage]; }
	auto GetRenderResource(ERenderImage inImage) { return m_Device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).Get(ResourceID(GetRenderRTID(inImage))); }

private:
	Device& m_Device;
	std::array<uint32_t, ERenderImage::NUM_IMAGES> m_RenderImages;
};

}