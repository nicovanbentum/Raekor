#pragma once

#include "Raekor/application.h"
#include "Raekor/scene.h"
#include "Raekor/assets.h"

#include "DXDevice.h"
#include "DXCommandList.h"
#include "DXRenderer.h"

namespace Raekor::DX {

struct BackbufferData {
    uint64_t                            mFenceValue;
    TextureID                           mBackBuffer;
    CommandList                         mCmdList;
};


class DXApp : public Application {
    using ShaderLibrary = std::unordered_map<std::string, ComPtr<IDxcBlob>>;

public:
    static constexpr uint32_t sFrameCount = 2;

    DXApp();
    ~DXApp();

    virtual void OnUpdate(float inDeltaTime) override;
    virtual void OnEvent(const SDL_Event& inEvent) override;

    void CompileShaders();
    void UploadSceneToGPU();
    void UploadBvhToGPU();

    DescriptorID QueueDirectStorageLoad(const TextureAsset::Ptr& inAsset, DXGI_FORMAT inFormat);

private:
    Scene m_Scene;
    Assets m_Assets;

    Device m_Device;
    Renderer m_Renderer;
    TextureID m_ImGuiFontTextureID;

    StagingHeap m_StagingHeap;
    ComPtr<IDXGISwapChain3> m_Swapchain;
    ComPtr<IDStorageQueue> m_StorageQueue;

    DescriptorID m_TLAS;
    ShaderLibrary m_Shaders;
    DescriptorID m_DefaultWhiteTexture;
    DescriptorID m_DefaultBlackTexture;
};


struct AccelerationStructure {
    BufferID buffer;
};


}