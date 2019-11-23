#pragma once

#include "pch.h"
#include "util.h"
#include "buffer.h"
#include "DXRenderer.h"

namespace Raekor {

template<typename T>
class DXResourceBuffer : public ResourceBuffer<T> {
public:
    DXResourceBuffer() {
        // describe the constant buffer, align the bytewidth to 16bit
        // TODO: Figure out if there's a better way to align, this seems like a hack
        D3D11_BUFFER_DESC cbdesc;
        cbdesc.Usage = D3D11_USAGE_DYNAMIC;
        cbdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cbdesc.MiscFlags = 0;
        cbdesc.ByteWidth = static_cast<UINT>(sizeof(T) + (16 - (sizeof(T) % 16)));
        cbdesc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA cbdata;
        cbdata.pSysMem = &data;
        cbdata.SysMemPitch = 0;
        cbdata.SysMemSlicePitch = 0;
        auto hr = D3D.device->CreateBuffer(&cbdesc, &cbdata, buffer.GetAddressOf());
        m_assert(SUCCEEDED(hr), "failed to create dx constant buffer");
    }

    void update(const T& resource) const override {
        // update the buffer's data on the GPU
        D3D11_MAPPED_SUBRESOURCE mapped_resource;
        D3D.context->Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
        memcpy(mapped_resource.pData, &resource, sizeof(T));
        D3D.context->Unmap(buffer.Get(), 0);
    }

    void bind(uint8_t slot) const override {
        D3D.context->VSSetConstantBuffers(slot, 1, buffer.GetAddressOf());
    }

private:
    T data;
    com_ptr<ID3D11Buffer> buffer;
};

} // namespace Raekor
