#version 450 core

layout(binding = 0) uniform ubo {
    mat4 lightMatrix;
    mat4 modelMatrix;
};

struct Vertex {
    float pos[3];
    float uv[2];
    float normal[3];
    float tangent[3];
};

layout(std430, binding = 1) buffer VertexBuffer {
    Vertex vertices[];
};

void main() {
    Vertex v = vertices[gl_VertexID];
    vec3 v_pos = vec3(v.pos[0], v.pos[1], v.pos[2]);
    gl_Position = lightMatrix * modelMatrix * vec4(v_pos, 1.0);
} 