#pragma once

#include "Editor.h"
#include "Widget.h"
#include "Engine/scene.h"
#include "Engine/assets.h"
#include "Engine/physics.h"
#include "Engine/Renderer/Device.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/CommandList.h"
#include "Engine/Renderer/RayTracedScene.h"
#include "Editor/Widgets/ProfileWidget.h"

namespace RK::DX12 {

class DXApp : public Editor
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
    TextureID  m_ImGuiFontTextureID;
    TextureID  m_DefaultWhiteTexture;
    TextureID  m_DefaultBlackTexture;
    TextureID  m_DefaultNormalTexture;

    Device          m_Device;
    Renderer        m_Renderer;
    RayTracedScene  m_RayTracedScene;
    RenderInterface m_RenderInterface;
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

class GPUProfileWidget : public IWidget
{
public:
    RTTI_DECLARE_VIRTUAL_TYPE(GPUProfileWidget);

    GPUProfileWidget(Editor* inEditor) : IWidget(inEditor, "GPU Profiler ") {}
    void Draw(Widgets* inWidgets, float inDeltaTime);
    void OnEvent(Widgets* inWidgets, const SDL_Event& inEvent);

private:
    float m_Zoom = 1.2f;
    String m_FilterInputBuffer;
    int m_SelectedSectionIndex = -1;
};

} // namespace Raekor::DX12