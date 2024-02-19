#pragma once

#include "pch.h"
#include "DXResource.h"

namespace Raekor::DX12 {

class Device;

class CommandList
{
public:
    CommandList() = default;
    CommandList(Device& inDevice, D3D12_COMMAND_LIST_TYPE inType, uint32_t inFrameIndex);

    operator ID3D12GraphicsCommandList* ()                { return m_CommandLists[m_CurrentCmdListIndex].Get(); }
    operator const ID3D12GraphicsCommandList* () const    { return m_CommandLists[m_CurrentCmdListIndex].Get(); }
    ID3D12GraphicsCommandList4* operator-> ()             { return m_CommandLists[m_CurrentCmdListIndex].Get(); }
    const ID3D12GraphicsCommandList4* operator-> () const { return m_CommandLists[m_CurrentCmdListIndex].Get(); }

    /* This will reset both the allocator and command lists, call this once per frame!! */
    void Reset();

    void Begin();
    void Close();

    inline uint64_t GetFenceValue() const { return m_SubmitFenceValue; }

    void ClearBuffer(Device& inDevice, BufferID inBuffer, Vec4 inValue);
    void ClearTexture(Device& inDevice, TextureID inTexture, Vec4 inValue);

    template<typename T> // omg c++ 20 concepts, much wow so cool
        requires ( sizeof(T) < sMaxRootConstantsSize&& std::default_initializable<T> )
    void PushGraphicsConstants(const T& inValue)
    {
        m_CommandLists[m_CurrentCmdListIndex]->SetGraphicsRoot32BitConstants(0, sizeof(T) / sizeof(DWORD), &inValue, 0);
    }

    template<typename T> // omg c++ 20 concepts, much wow so cool
        requires ( sizeof(T) < sMaxRootConstantsSize&& std::default_initializable<T> )
    void PushComputeConstants(const T& inValue)
    {
        m_CommandLists[m_CurrentCmdListIndex]->SetComputeRoot32BitConstants(0, sizeof(T) / sizeof(DWORD), &inValue, 0);
    }

    /* Only buffers are allowed to be used as root descriptor. */
    void BindToSlot(Buffer& inBuffer, EBindSlot inSlot, uint32_t inOffset = 0);

    /* Helper function to bind a mesh. */
    void BindVertexAndIndexBuffers(Device& inDevice, const Mesh& inMesh);

    /* Sets both the viewport and scissor to the size defined by inViewport. */
    void SetViewportAndScissor(const Viewport& inViewport);

    void Submit(Device& inDevice, ID3D12CommandQueue* inQueue);

    void TrackResource(const Buffer& inBuffer) { m_ResourceRefs.push_back(inBuffer); }
    void TrackResource(const Texture& inTexture) { m_ResourceRefs.push_back(inTexture); }

    void ReleaseTrackedResources() { m_ResourceRefs.clear(); }

    uint32_t GetFrameIndex() const { return m_FrameIndex; }

private:
    uint32_t m_FrameIndex = 0;
    uint64_t m_SubmitFenceValue = 0;
    uint32_t m_CurrentCmdListIndex = 0;
    ComPtr<ID3D12Fence> m_Fence = nullptr;
    std::vector<DeviceResource> m_ResourceRefs;
    std::vector<ComPtr<ID3D12GraphicsCommandList4>> m_CommandLists;
    std::vector<ComPtr<ID3D12CommandAllocator>> m_CommandAllocators;
};

}
