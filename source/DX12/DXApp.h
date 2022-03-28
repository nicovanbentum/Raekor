#pragma once

#include "DXDevice.h"
#include "Raekor/application.h"
#include "Raekor/scene.h"

#include "DXBlit.h"
#include "DXGBuffer.h"

namespace Raekor::DX {

class DXApp : public Application {
    using ShaderLibrary = std::unordered_map<std::string, ComPtr<IDxcBlob>>;

public:
    struct {
        glm::vec4 albedo;
        glm::uvec4 textures;
        glm::mat4 mViewProj;
    } m_RootConstants;

    static constexpr uint32_t sFrameCount = 3;

    DXApp();
    ~DXApp();

    virtual void OnUpdate(float dt) override;
    virtual void OnEvent(const SDL_Event& event) override;

    uint32_t UploadTextureFromFile(const TextureAsset::Ptr& asset, const fs::path& path, DXGI_FORMAT format);

private:
    Scene m_Scene;
    Assets m_Assets;

    Device m_Device;
    ComPtr<IDXGISwapChain3> m_Swapchain;
    ComPtr<IDStorageQueue> m_StorageQueue;

    uint32_t m_DefaultWhiteTexture;
    uint32_t m_DefaultBlackTexture;

    BlitPass m_Blit;
    GBufferPass m_GBuffer;
    ShaderLibrary m_Shaders;
    std::vector<ComPtr<ID3D12GraphicsCommandList>> m_CommandLists;
    std::vector<ComPtr<ID3D12CommandAllocator>> m_CommnadAllocators;
    
    uint32_t m_FrameIndex = 0;
    HANDLE m_FenceEvent;
    ComPtr<ID3D12Fence> m_Fence;
    std::vector<UINT64> m_FenceValues;
};


}