#pragma once

#include "pch.h"
#include "shader.h"
#include "util.h"

namespace Raekor {

class DXShader : public Shader {
public:
	DXShader(std::string vertex, std::string pixel);
	const void bind() const override;
	const void unbind() const override;

	com_ptr<ID3D11VertexShader> vertex_shader;
	com_ptr<ID3D11PixelShader> pixel_shader;
	com_ptr<ID3D10Blob> vertex_shader_buffer;
	com_ptr<ID3D10Blob> pixel_shader_buffer;
};

} // namespace Raekor