#version 450

layout(location = 0) in vec2 uv;

layout(binding = 0) uniform sampler2D source;

layout(location = 0) out vec4 final_color;

void main() {

    final_color = texture(source, uv);
}