#pragma once

#include "pch.h"
#include "DXResource.h"

namespace Raekor::DX {

class Device;

class CommandList {
public:
	CommandList() = default;
	CommandList(Device& inDevice);

	operator ID3D12GraphicsCommandList* ()					{ return m_CommandLists[m_CurrentCmdListIndex].Get(); }
	operator const ID3D12GraphicsCommandList* () const		{ return m_CommandLists[m_CurrentCmdListIndex].Get(); }
	ID3D12GraphicsCommandList4* operator-> ()				{ return m_CommandLists[m_CurrentCmdListIndex].Get(); }
	const ID3D12GraphicsCommandList4* operator-> () const	{ return m_CommandLists[m_CurrentCmdListIndex].Get(); }

	/* This will reset both the allocator and command lists, call this once per frame!! */
	void Reset();
	
	void Begin();
	void Close();

	void Push();
	void Pop();

	/* Resource API. Typically used for updating resources that are in GPU memory. */
	void UpdateBuffer(Buffer& inDstBuffer, uint32_t inDstOffset, uint32_t inDstSize, void* inDataPtr);
	void UpdateTexture(Texture& inDstTexture, uint32_t inDstMip, void* inDataPtr);

	/* Execute API. */
	void Draw();
	void Dispatch(uint32_t inSizeX, uint32_t inSizeY, uint32_t inSizeZ);
	void DispatchRays(uint32_t inSizeX, uint32_t inSizeY);

	template<typename T> // omg c++ 20 concepts, much wow so cool
		requires (sizeof(T) < sMaxRootConstantsSize && std::default_initializable<T>)
	void PushConstants(const T& inValue) {
		m_CommandLists[m_CurrentCmdListIndex]->SetGraphicsRoot32BitConstants(0, sizeof(T) / sizeof(DWORD), &inValue, 0);
	}

	/* Only buffers are allowed to be used as root descriptor. */
	void BindToSlot(Buffer& inBuffer, EBindSlot inSlot, uint32_t inOffset = 0);

	/* Sets both the viewport and scissor to the size defined by inViewport. */
	void SetViewportScissorRect(const Viewport& inViewport);

	void Submit(Device& inDevice);

private:
	uint32_t m_CurrentCmdListIndex = 0;
	ComPtr<ID3D12Fence> m_Fence = nullptr;
	std::vector<ComPtr<ID3D12GraphicsCommandList4>> m_CommandLists;
	std::vector<ComPtr<ID3D12CommandAllocator>> m_CommandAllocators;
};

}
