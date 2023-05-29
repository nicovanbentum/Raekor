#include "pch.h"
#include "DXShader.h"
#include "DXRenderer.h"

namespace Raekor {

DXShader::DXShader(const Stage* stages, size_t stageCount) {
    m_stages.resize(stageCount);

    for (uint32_t i = 0; i < stageCount; i++) {
        m_stages[i] = stages[i];
        CompileStage(m_stages[i]);

        auto error_code = std::error_code();
        m_stages[i].updatetime = FileSystem::last_write_time(m_stages[i].textfile, error_code);
    }
}


void DXShader::CheckForReload() {
    for (auto& stage : m_stages) {
        auto error_code = std::error_code();
        auto timestamp = FileSystem::last_write_time(stage.textfile, error_code);

        while (error_code)
            timestamp = FileSystem::last_write_time(stage.textfile, error_code);

        if (timestamp > stage.updatetime) {
            CompileStage(stage);
            stage.updatetime = timestamp;
        }
    }
}


bool DXShader::CompileStage(const Stage& inStage) {
    const auto fp = std::string(inStage.textfile);
    const auto ww = std::wstring(fp.begin(), fp.end());
    const auto wstr = ww.c_str();

    ComPtr<ID3D10Blob> buffer, errors;
    auto hr = D3DCompileFromFile(wstr, nullptr, nullptr, "main", sTypeTargets[inStage.type], 0, 0, buffer.GetAddressOf(), errors.GetAddressOf());

    if (FAILED(hr)) {
        if (errors)
            OutputDebugStringA((char*)errors->GetBufferPointer());
        
        std::cout << "Compilation " << COUT_RED("failed") << " for shader: " << fp << '\n';
        return false;
    }

    switch (inStage.type) {
        case Type::VERTEX: {
            hr = D3D.device->CreateVertexShader(buffer.Get()->GetBufferPointer(), buffer->GetBufferSize(), NULL, vertex_shader.GetAddressOf());
        } break;
        case Type::FRAG: {
            hr = D3D.device->CreatePixelShader(buffer.Get()->GetBufferPointer(), buffer->GetBufferSize(), NULL, pixel_shader.GetAddressOf());
        } break;
        case Type::GEO: {
            hr = D3D.device->CreateGeometryShader(buffer.Get()->GetBufferPointer(), buffer->GetBufferSize(), NULL, geo_shader.GetAddressOf());
        } break;
        case Type::COMPUTE: {
            hr = D3D.device->CreateComputeShader(buffer.Get()->GetBufferPointer(), buffer->GetBufferSize(), NULL, compute_shader.GetAddressOf());
        } break;
    }

    if (!SUCCEEDED(hr)) {
        std::cout << "Compilation " << COUT_RED("failed") << " for shader: " << fp << '\n';
        return false;
    }
 
    std::cout << "Compilation " << COUT_GREEN("succeeded") << " for shader: " << fp << '\n';
    return true;
}


void DXShader::Bind() {
    if (vertex_shader)
        D3D.context->VSSetShader(vertex_shader.Get(), NULL, 0);
    if (pixel_shader)
        D3D.context->PSSetShader(pixel_shader.Get(), NULL, 0);
    if (geo_shader)
        D3D.context->GSSetShader(geo_shader.Get(), NULL, 0);
    if (compute_shader)
        D3D.context->CSSetShader(compute_shader.Get(), NULL, 0);
}


} // namespace Raekor