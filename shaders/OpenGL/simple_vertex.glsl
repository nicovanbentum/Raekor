#version 330 core

layout(location = 0) in vec3 vertex_pos;
layout(location = 1) in vec2 vertex_uv;

layout (std140) uniform Camera {
	mat4 model;
	mat4 view;
	mat4 proj;
};

//we send out a uv coordinate for our frag shader
out vec2 uv;

//4x4 matrix for our model view camera
uniform mat4 MVP;

void main()
{
    vec3 worldPos = vec3(model * vec4(vertex_pos, 1.0));
    gl_Position = proj * view * vec4(worldPos, 1.0);

    // set output uv
    uv = vertex_uv;
}