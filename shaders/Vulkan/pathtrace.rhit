#version 450
#extension GL_ARB_separate_shader_objects : enable


layout (set = 0, binding = 0) uniform MeshData {
	mat4 modelMatrix;
};

layout(set = 0, binding = 2) uniform FrameData {
    mat4 viewMatrix;
	mat4 projMatrix;
	vec4 light_position;
	vec4 light_angle;
};

// vertex attributes
layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 normal;

// fragment shader
layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec3 out_pos;

void main() {
	// CAMERA SPACE : VERTEX POSITION
    gl_Position = projMatrix * viewMatrix * modelMatrix * vec4(pos, 1.0);

	out_pos = vec3(modelMatrix* vec4(pos, 1.0));
	out_normal = mat3(transpose(inverse(modelMatrix))) * normal;
	out_uv = uv;
}