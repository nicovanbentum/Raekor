#pragma once

#include "pch.h"

namespace Raekor::DX {

class BindlessBuffers {
public:


private:
	ComPtr<ID3D12DescriptorHeap> heap;
	std::vector<ComPtr<ID3D12Resource>> resources;
};

}