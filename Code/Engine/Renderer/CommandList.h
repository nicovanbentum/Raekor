#pragma once

#include "pch.h"
#include "Resource.h"

namespace RK {
struct Mesh;
class Viewport;
}

namespace RK::DX12 {

class Device;

class CommandList
{
public:
    CommandList() = default;
    CommandList(Device& inDevice, D3D12_COMMAND_LIST_TYPE inType);
    CommandList(Device& inDevice, D3D12_COMMAND_LIST_TYPE inType, uint32_t inFrameIndex);

    operator ID3D12GraphicsCommandList* ()                { return m_CommandList.Get(); }
    operator const ID3D12GraphicsCommandList* () const    { return m_CommandList.Get(); }
    ID3D12GraphicsCommandList4* operator-> ()             { return m_CommandList.Get(); }
    const ID3D12GraphicsCommandList4* operator-> () const { return m_CommandList.Get(); }

    void Reset();
    void Begin();
    void Close();
    
    uint32_t GetFrameIndex() const { return m_FrameIndex; }
    uint64_t GetFenceValue() const { return m_SubmittedFenceValue; }

    void SetViewportAndScissor(Texture& inTexture);
    void SetViewportAndScissor(const Viewport& inViewport);

    void ClearBuffer(Device& inDevice, BufferID inBuffer, Vec4 inValue);
    void ClearTexture(Device& inDevice, TextureID inTexture, Vec4 inValue);

    void BindDefaults(Device& inDevice);
    void BindIndexBuffer(Buffer& inBuffer);
    void BindVertexAndIndexBuffers(Device& inDevice, const RK::Mesh& inMesh);

    template<typename T> requires ( sizeof(T) < sMaxRootConstantsSize && std::default_initializable<T> )
    void PushGraphicsConstants(const T& inValue);
    
    template<typename T> requires ( sizeof(T) < sMaxRootConstantsSize&& std::default_initializable<T> )
    void PushComputeConstants(const T& inValue);

    void BindToSlot(Buffer& inBuffer, EBindSlot inSlot, uint32_t inOffset = 0);

    void Submit(Device& inDevice, ID3D12CommandQueue* inQueue);

private:
    uint32_t m_FrameIndex = 0;
    uint64_t m_SubmittedFenceValue = 0;
    ComPtr<ID3D12Fence> m_Fence = nullptr;
    Array<DeviceResource> m_ResourceRefs;
    ComPtr<ID3D12GraphicsCommandList4> m_CommandList;
    ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
};

template<typename T> requires ( sizeof(T) < sMaxRootConstantsSize&& std::default_initializable<T> )
    void CommandList::PushGraphicsConstants(const T& inValue)
{
    m_CommandList->SetGraphicsRoot32BitConstants(0, sizeof(T) / sizeof(DWORD), &inValue, 0);
}

template<typename T> // omg c++ 20 concepts, much wow so cool
    requires ( sizeof(T) < sMaxRootConstantsSize&& std::default_initializable<T> )
void CommandList::PushComputeConstants(const T& inValue)
{
    m_CommandList->SetComputeRoot32BitConstants(0, sizeof(T) / sizeof(DWORD), &inValue, 0);
}

}
