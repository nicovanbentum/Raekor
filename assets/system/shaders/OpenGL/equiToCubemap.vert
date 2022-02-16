#version 450

layout(location = 0) in vec3 pos;

layout(location = 0) out vec3 localPos;

layout(binding = 0) uniform Uniforms {
    mat4 view;
    mat4 projection;
};

void main() {
    localPos = pos;
    gl_Position = projection * view * vec4(localPos, 1.0);
}