#version 450

layout(location = 0) in vec3 pos;

uniform mat4 projection;
uniform mat4 view;

out VS_OUT {
    float lerp;
} vs_out;

void main() {

    gl_Position = projection * view * vec4(pos, 1.0);
    gl_Position = gl_Position.xyww;
    vs_out.lerp = pos.y;
}