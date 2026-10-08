#pragma once

#include "Editor.h"
#include "Widget.h"
#include "Engine/scene.h"
#include "Engine/assets.h"
#include "Engine/physics.h"
#include "Engine/Renderer/RenderSystem.h"
#include "Engine/Renderer/CommandList.h"

namespace RK::DX12 {

class DXApp : public Editor
{
public:
    DXApp();
    virtual ~DXApp();

    virtual void OnUpdate(float inDeltaTime) override;
    virtual void OnEvent(const SDL_Event& inEvent) override;

    Device& GetDevice() { return m_RenderSystem.GetDevice(); }
    Renderer& GetRenderer() { return m_RenderSystem.GetRenderer(); }
    IRenderInterface* GetRenderInterface() { return &m_RenderSystem; }

private:
    RenderSystem m_RenderSystem;
};


class DeviceResourcesWidget : public IWidget
{
public:
    RTTI_DECLARE_VIRTUAL_TYPE(DeviceResourcesWidget);

    DeviceResourcesWidget(Editor* inEditor) : IWidget(inEditor, "GPU Resources ") {}
    void Draw(Widgets* inWidgets, float inDeltaTime);
    void OnEvent(Widgets* inWidgets, const SDL_Event& inEvent) {}

private:
    uint64_t m_BuffersTotalSize = 0;
    uint64_t m_TexturesTotalSize = 0;
    uint16_t m_BufferUsageFilter = 0xFFFF;
    uint16_t m_TextureUsageFilter = 0xFFFF;
    HashSet<const ID3D12Resource*> m_SeenResources;
};

} // namespace Raekor::DX12