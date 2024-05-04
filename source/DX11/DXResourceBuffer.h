#pragma once

#include "pch.h"
#include "DXRenderer.h"

namespace RK {

class DXResourceBuffer
{
public:
    ~DXResourceBuffer();
    DXResourceBuffer(size_t size);
    DXResourceBuffer() = default;

    void Update(void* data, const size_t size);
    ID3D11Buffer* GetBuffer() const { return buffer.Get(); }

private:
    void* mappedData;
    D3D11_MAPPED_SUBRESOURCE resource;
    ComPtr<ID3D11Buffer> buffer;
};

} // namespace Raekor
