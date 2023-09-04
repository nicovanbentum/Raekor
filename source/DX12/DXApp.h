#pragma once

#include "Raekor/editor.h"
#include "Raekor/scene.h"
#include "Raekor/physics.h"
#include "Raekor/assets.h"
#include "Raekor/widget.h"

#include "DXScene.h"
#include "DXDevice.h"
#include "DXRenderer.h"
#include "DXCommandList.h"

namespace Raekor::DX12 {

class DXApp : public IEditor
{
public:
	DXApp();
	~DXApp();

	virtual void OnUpdate(float inDeltaTime) override;
	virtual void OnEvent(const SDL_Event& inEvent) override;

	Device& GetDevice() { return m_Device; }
	Renderer& GetRenderer() { return m_Renderer; }
	IRenderInterface* GetRenderInterface() { return &m_RenderInterface; }

private:
	DescriptorID UploadTextureDirectStorage(const TextureAsset::Ptr& inAsset, DXGI_FORMAT inFormat); // unused for now

private:
	TextureID     m_ImGuiFontTextureID;
	DescriptorID  m_DefaultWhiteTexture;
	DescriptorID  m_DefaultBlackTexture;
	DescriptorID  m_DefaultNormalTexture;

	ComPtr<IDXGISwapChain3> m_Swapchain;
	ComPtr<IDStorageQueue>  m_FileStorageQueue;
	ComPtr<IDStorageQueue>  m_MemoryStorageQueue;

	Device          m_Device;
	Renderer        m_Renderer;
	StagingHeap     m_StagingHeap;
	RayTracedScene  m_RayTracedScene;
	RenderInterface m_RenderInterface;
};


class DebugWidget : public IWidget
{
public:
	RTTI_DECLARE_TYPE(DebugWidget);

	DebugWidget(Application* inApp);

	virtual void Draw(Widgets* inWidgets, float dt) override;
	virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override;

private:
	Device& m_Device;
	Renderer& m_Renderer;
};

} // namespace Raekor::DX12