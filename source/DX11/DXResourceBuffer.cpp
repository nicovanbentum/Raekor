#include "pch.h"
#include "DXResourceBuffer.h"

namespace Raekor {

DXResourceBuffer::~DXResourceBuffer() {
    //D3D.context->Unmap(buffer.Get(), 0);
}

DXResourceBuffer::DXResourceBuffer(size_t size) {
    // describe the constant buffer, align the bytewidth to 16bit
    // TODO: Figure out if there's a better way to align, this seems like a hack
    D3D11_BUFFER_DESC cbdesc;
    cbdesc.Usage = D3D11_USAGE_DYNAMIC;
    cbdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbdesc.MiscFlags = 0;
    cbdesc.ByteWidth = (UINT)gAlignUp(size, 16);
    cbdesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA cbdata;
    cbdata.pSysMem = &mappedData;
    cbdata.SysMemPitch = 0;
    cbdata.SysMemSlicePitch = 0;
    auto hr = D3D.device->CreateBuffer(&cbdesc, &cbdata, buffer.GetAddressOf());
    assert(SUCCEEDED(hr) && "failed to create dx constant buffer");

}

void DXResourceBuffer::Update(void* data, const size_t size) {
    auto hr = D3D.context->Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
    assert(SUCCEEDED(hr) && "failed to map dx constant buffer");

    // update the buffer's data on the GPU by memcpying from the mapped CPU memory to VRAM
    memcpy(resource.pData, data, size);

    // Unmap is required in DX11 to allow it to be bound to a shader
    D3D.context->Unmap(buffer.Get(), 0);
}

}