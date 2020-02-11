#pragma once

#include "pch.h"
#include "util.h"
#include "buffer.h"
#include "DXRenderer.h"

namespace Raekor {

class DXResourceBuffer : public ResourceBuffer {
public:
    ~DXResourceBuffer();
    DXResourceBuffer(size_t size);

    void update(void* data, const size_t size) const override;
    void bind(uint8_t slot) const override;

private:
    void* mappedData;
    D3D11_MAPPED_SUBRESOURCE resource;
    com_ptr<ID3D11Buffer> buffer;
};

} // namespace Raekor
