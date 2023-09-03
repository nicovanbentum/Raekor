#pragma once

#include "Raekor/editor.h"
#include "Raekor/scene.h"
#include "Raekor/physics.h"
#include "Raekor/assets.h"
#include "Raekor/widget.h"

#include "DXDevice.h"
#include "DXCommandList.h"
#include "DXRenderer.h"

namespace Raekor::DX12 {

struct BackbufferData {
    uint64_t    mFenceValue;
    TextureID   mBackBuffer;
    CommandList mCmdList;
};

class DXApp : public IEditor {
public:
    DXApp();
    ~DXApp();

    virtual void OnUpdate(float inDeltaTime) override;
    virtual void OnEvent(const SDL_Event& inEvent) override;

    Device& GetDevice()                     { return m_Device; }
    Renderer& GetRenderer()                 { return m_Renderer; }
    IRenderInterface* GetRenderInterface()  { return &m_RenderInterface; }

    void UploadTopLevelBVH(CommandList& inCmdList);
    void UploadBindlessSceneBuffers(CommandList& inCmdList);

private:
    DescriptorID UploadTextureDirectStorage(const TextureAsset::Ptr& inAsset, DXGI_FORMAT inFormat); // unused for now

private:
    Device          m_Device;
    Renderer        m_Renderer;
    RenderInterface m_RenderInterface;

    TextureID m_ImGuiFontTextureID;

    StagingHeap             m_StagingHeap;
    ComPtr<IDXGISwapChain3> m_Swapchain;
    ComPtr<IDStorageQueue>  m_FileStorageQueue;
    ComPtr<IDStorageQueue>  m_MemoryStorageQueue;

    BufferID      m_TLASBuffer;
    BufferID      m_InstancesBuffer;
    BufferID      m_MaterialsBuffer;
    DescriptorID  m_TLASDescriptor;

    DescriptorID  m_DefaultWhiteTexture;
    DescriptorID  m_DefaultBlackTexture;
    DescriptorID  m_DefaultNormalTexture;
};


class DebugWidget : public IWidget {
public:
    RTTI_DECLARE_TYPE(DebugWidget);

    DebugWidget(Application* inApp);

    virtual void Draw(Widgets* inWidgets, float dt) override;
    virtual void OnEvent(Widgets* inWidgets, const SDL_Event& ev) override;

private:
    Device& m_Device;
    Renderer& m_Renderer;
};


}