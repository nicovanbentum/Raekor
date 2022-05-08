#pragma once

#include "DXDevice.h"
#include "Raekor/application.h"
#include "Raekor/scene.h"

#include "DXBlit.h"
#include "DXShadows.h"
#include "DXGBuffer.h"

namespace Raekor::DX {

struct BackbufferData {
    uint64_t                            mFenceValue;
    uint32_t                            mBackbufferRTV;
    ComPtr<ID3D12GraphicsCommandList4>  mCmdList;
    ComPtr<ID3D12CommandAllocator>      mCmdAllocator;
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

    uint32_t QueueDirectStorageLoad(const TextureAsset::Ptr& asset, const fs::path& path, DXGI_FORMAT format);

private:
    Scene m_Scene;
    Assets m_Assets;

    Device m_Device;
    ComPtr<IDXGISwapChain3> m_Swapchain;
    ComPtr<IDStorageQueue> m_StorageQueue;

    uint32_t m_TLAS;
    uint32_t m_DefaultWhiteTexture;
    uint32_t m_DefaultBlackTexture;

    PresentPass m_Blit;
    ShadowPass m_Shadows;
    GBufferPass m_GBuffer;
    ShaderLibrary m_Shaders;
    
    HANDLE m_FenceEvent;
    uint32_t m_FrameIndex = 0;
    ComPtr<ID3D12Fence> m_Fence;
    std::array<BackbufferData, sFrameCount> m_BackbufferData;
};


struct AccelerationStructure {
    uint32_t handle = 0;
};


}