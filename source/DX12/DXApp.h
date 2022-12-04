#pragma once

#include "Raekor/application.h"
#include "Raekor/scene.h"
#include "Raekor/assets.h"

#include "DXDevice.h"
#include "DXCommandList.h"
#include "DXRenderGraph.h"

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

    virtual void OnUpdate(float dt) override;
    virtual void OnEvent(const SDL_Event& event) override;
    
    BackbufferData& GetBackbufferData() { return m_BackbufferData[m_FrameIndex]; }
    BackbufferData& GetPrevBackbufferData() { return m_BackbufferData[!m_FrameIndex]; }

    void CompileShaders();
    void UploadSceneToGPU();
    void UploadBvhToGPU();

    ResourceID QueueDirectStorageLoad(const TextureAsset::Ptr& asset, DXGI_FORMAT format);
    ResourceID UploadMaterialTexture(const TextureAsset::Ptr& inAsset, DXGI_FORMAT inFormat);

private:
    Scene m_Scene;
    Assets m_Assets;

    Device m_Device;
    RenderGraph m_RenderGraph;
    
    FfxFsr2Context m_Fsr2;
    uint32_t m_FsrFrameCounter = 0;
    TextureID m_FsrOutputTexture;
    std::vector<uint8_t> m_FsrScratchMemory;

    StagingHeap m_StagingHeap;
    ComPtr<IDXGISwapChain3> m_Swapchain;
    ComPtr<IDStorageQueue> m_StorageQueue;

    ResourceID m_TLAS;
    ResourceID m_DefaultWhiteTexture;
    ResourceID m_DefaultBlackTexture;

    ShaderLibrary m_Shaders;

    HANDLE m_FenceEvent;
    uint32_t m_FrameIndex = 0;
    ComPtr<ID3D12Fence> m_Fence;
    std::array<BackbufferData, sFrameCount> m_BackbufferData;
};


struct AccelerationStructure {
    BufferID buffer;
};


}