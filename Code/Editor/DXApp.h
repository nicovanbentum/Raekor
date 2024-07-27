#pragma once

#include "editor.h"
#include "Engine/scene.h"
#include "Engine/physics.h"
#include "Engine/assets.h"
#include "Engine/widget.h"

#include "Engine/Renderer/DXScene.h"
#include "Engine/Renderer/DXDevice.h"
#include "Engine/Renderer/DXRenderer.h"
#include "Engine/Renderer/DXCommandList.h"

namespace RK::DX12 {

class DXApp : public IEditor
{
public:
    DXApp();
    ~DXApp();

    virtual void OnUpdate(float inDeltaTime) override;
    virtual void OnEvent(const SDL_Event& inEvent) override;

    Device& GetDevice() { return m_Device; }
    Renderer& GetRenderer() { return m_Renderer; }
    RayTracedScene& GetRayTracedScene() { return m_RayTracedScene; }
    IRenderInterface* GetRenderInterface() { return &m_RenderInterface; }

private:
    DescriptorID UploadTextureDirectStorage(const TextureAsset::Ptr& inAsset, DXGI_FORMAT inFormat); // unused for now

private:
    TextureID  m_ImGuiFontTextureID;
    TextureID  m_DefaultWhiteTexture;
    TextureID  m_DefaultBlackTexture;
    TextureID  m_DefaultNormalTexture;

    ComPtr<IDStorageQueue>  m_FileStorageQueue;
    ComPtr<IDStorageQueue>  m_MemoryStorageQueue;

    Device          m_Device;
    Renderer        m_Renderer;
    RayTracedScene  m_RayTracedScene;
    RenderInterface m_RenderInterface;
};


class DeviceResourcesWidget : public IWidget
{
public:
    DeviceResourcesWidget(Application* inApp) : IWidget(inApp, "GPU Resources ") {}
    void Draw(Widgets* inWidgets, float inDeltaTime);
    void OnEvent(Widgets* inWidgets, const SDL_Event& inEvent) {}

private:
    uint16_t m_BufferUsageFilter = 0xFFFF;
    uint16_t m_TextureUsageFilter = 0xFFFF;
    HashSet<ID3D12Resource*> m_UniqueResources;
};

} // namespace Raekor::DX12