#version 450

layout(location = 0) in vec3 pos;

layout(binding = 0) uniform Uniforms {
    mat4 projection;
    mat4 view;
    vec3 mid_color;
    vec3 top_color;
};

layout(location = 0) out VS_OUT {
    float lerp;
} vs_out;

void main() {

    gl_Position = projection * view * vec4(pos, 1.0);
    gl_Position = gl_Position.xyww;
    vs_out.lerp = pos.y;
}