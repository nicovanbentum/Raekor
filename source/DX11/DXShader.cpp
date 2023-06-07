#include "pch.h"
#include "DXShader.h"
#include "DXRenderer.h"

namespace Raekor {

DXShader::DXShader(const Stage* stages, size_t stageCount) {
    m_Stages.resize(stageCount);

    for (uint32_t i = 0; i < stageCount; i++) {
        m_Stages[i] = stages[i];
        CompileStage(m_Stages[i]);

        auto error_code = std::error_code();
        m_Stages[i].mUpdateTime = FileSystem::last_write_time(m_Stages[i].mTextFile, error_code);
    }
}


void DXShader::CheckForReload() {
    for (auto& stage : m_Stages) {
        auto error_code = std::error_code();
        auto timestamp = FileSystem::last_write_time(stage.mTextFile, error_code);

        while (error_code)
            timestamp = FileSystem::last_write_time(stage.mTextFile, error_code);

        if (timestamp > stage.mUpdateTime) {
            CompileStage(stage);
            stage.mUpdateTime = timestamp;
        }
    }
}


bool DXShader::CompileStage(const Stage& inStage) {
    const auto fp = std::string(inStage.mTextFile);
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
            hr = D3D.device->CreateVertexShader(buffer.Get()->GetBufferPointer(), buffer->GetBufferSize(), NULL, m_VertexShader.GetAddressOf());
        } break;
        case Type::FRAG: {
            hr = D3D.device->CreatePixelShader(buffer.Get()->GetBufferPointer(), buffer->GetBufferSize(), NULL, m_PixelShader.GetAddressOf());
        } break;
        case Type::GEO: {
            hr = D3D.device->CreateGeometryShader(buffer.Get()->GetBufferPointer(), buffer->GetBufferSize(), NULL, m_GeometryShader.GetAddressOf());
        } break;
        case Type::COMPUTE: {
            hr = D3D.device->CreateComputeShader(buffer.Get()->GetBufferPointer(), buffer->GetBufferSize(), NULL, m_ComputeShader.GetAddressOf());
        } break;
    }

    if (!SUCCEEDED(hr)) {
        std::cout << "Compilation " << COUT_RED("failed") << " for shader: " << fp << '\n';
        return false;
    }
 
    std::cout << "Compilation " << COUT_GREEN("succeeded") << " for shader: " << fp << '\n';
    return true;
}


void DXShader::Bind(ID3D11DeviceContext* inContext) {
    if (m_VertexShader)
        inContext->VSSetShader(m_VertexShader.Get(), NULL, 0);
    if (m_PixelShader)
        inContext->PSSetShader(m_PixelShader.Get(), NULL, 0);
    if (m_GeometryShader)
        inContext->GSSetShader(m_GeometryShader.Get(), NULL, 0);
    if (m_ComputeShader)
        inContext->CSSetShader(m_ComputeShader.Get(), NULL, 0);
}


} // namespace Raekor