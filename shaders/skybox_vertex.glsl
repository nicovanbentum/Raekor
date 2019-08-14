#version 330 core
layout (location = 0) in vec3 pos;

out vec3 texture_pos;

layout (std140) uniform Camera {
	mat4 model_view_projection;
};

void main()
{
    texture_pos = pos;
    gl_Position = model_view_projection * vec4(pos, 1.0);
}  