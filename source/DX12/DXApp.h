#pragma once

#include "Raekor/application.h"
#include "Raekor/scene.h"
#include "Raekor/assets.h"

#include "Editor/widget.h"

#include "DXDevice.h"
#include "DXCommandList.h"
#include "DXRenderer.h"

namespace Raekor::DX12 {

struct BackbufferData {
    uint64_t    mFenceValue;
    TextureID   mBackBuffer;
    CommandList mCmdList;
};

struct ShaderProgram {
    Path mVertexShaderFilePath;
    Path mPixelShaderFilePath;
    Path mComputeShaderFilePath;
};


struct SystemShadersDX12 {
    RTTI_CLASS_HEADER(SystemShadersDX12);

    Path GBufferShader;
    Path GrassShader;
    Path RTShadowsShader;
    Path RTAOShader;
    Path RTReflectionsShader;
};

class DXApp : public Application {
public:
    friend class IWidget;

    DXApp();
    ~DXApp();

    virtual void OnUpdate(float inDeltaTime) override;
    virtual void OnEvent(const SDL_Event& inEvent) override;

    template<typename T> std::shared_ptr<T>  GetWidget();
    const auto& GetWidgets() { return m_Widgets; }

    virtual Scene* GetScene() { return &m_Scene; }
    virtual Assets* GetAssets() { return &m_Assets; }
    virtual Physics* GetPhysics() { return &m_Physics; }
    virtual IRenderer* GetRenderer() { return &m_RenderInterface; }

    void LogMessage(const std::string& inMessage) override;

    virtual void SetActiveEntity(Entity inEntity) { m_ActiveEntity = inEntity; }
    virtual Entity GetActiveEntity() { return m_ActiveEntity; }

    void CompileShaders();
    void UploadBvhToGPU();



private:
    DescriptorID UploadTextureDirectStorage(const TextureAsset::Ptr& inAsset, DXGI_FORMAT inFormat); // unused for now

private:
    Scene  m_Scene;
    Assets m_Assets;
    Physics m_Physics;

    Device          m_Device;
    Renderer        m_Renderer;
    RenderInterface m_RenderInterface;

    Entity m_ActiveEntity = sInvalidEntity;
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

    std::vector<std::string> m_Messages;
    std::vector<std::shared_ptr<IWidget>> m_Widgets;
};


template<typename T>
std::shared_ptr<T> DXApp::GetWidget() {
    for (const auto& widget : m_Widgets)
        if (widget->GetRTTI() == gGetRTTI<T>())
            return std::static_pointer_cast<T>(widget);

    return nullptr;
}


}