#pragma once

#include "pch.h"
#include "shader.h"
#include "util.h"

namespace Raekor {

class DXShader : public Shader {
public:
    DXShader(Stage* stages, size_t stageCount);
    const void bind() const override;
    const void unbind() const override;

private:
    com_ptr<ID3D11VertexShader> vertex_shader;
    com_ptr<ID3D11PixelShader> pixel_shader;
    com_ptr<ID3D11ComputeShader> compute_shader;
    com_ptr<ID3D11GeometryShader> geo_shader;
};

} // namespace Raekor