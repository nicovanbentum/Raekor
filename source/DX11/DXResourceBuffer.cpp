#include "pch.h"
#include "DXResourceBuffer.h"

namespace Raekor {

DXResourceBuffer::~DXResourceBuffer() {
    D3D.context->Unmap(buffer.Get(), 0);
}

DXResourceBuffer::DXResourceBuffer(size_t size) {
    // describe the constant buffer, align the bytewidth to 16bit
    // TODO: Figure out if there's a better way to align, this seems like a hack
    D3D11_BUFFER_DESC cbdesc;
    cbdesc.Usage = D3D11_USAGE_DYNAMIC;
    cbdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbdesc.MiscFlags = 0;
    cbdesc.ByteWidth = static_cast<UINT>(size + (16 - (size % 16)));
    cbdesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA cbdata;
    cbdata.pSysMem = &mappedData;
    cbdata.SysMemPitch = 0;
    cbdata.SysMemSlicePitch = 0;
    auto hr = D3D.device->CreateBuffer(&cbdesc, &cbdata, buffer.GetAddressOf());
    m_assert(SUCCEEDED(hr), "failed to create dx constant buffer");
    D3D.context->Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
}

void DXResourceBuffer::update(void* data, const size_t size) const {
    // update the buffer's data on the GPU by memcpying from the mapped CPU memory to VRAM
    memcpy(resource.pData, data, size);
}

void DXResourceBuffer::bind(uint8_t slot) const {
    D3D.context->VSSetConstantBuffers(slot, 1, buffer.GetAddressOf());
}
}