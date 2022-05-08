#pragma once

#include "pch.h"

namespace Raekor::DX {

class CommandList {
public:
	ID3D12GraphicsCommandList* operator->() { return m_CommandList.Get(); }
	void UpdateBuffer(ID3D12Resource* inDstBuffer, uint32_t inDstOffset, uint32_t inDstSize, void* inDataPtr);

private:
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;
};

}
