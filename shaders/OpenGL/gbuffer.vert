#version 460 core

// vertex buffer data
layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_normal;
layout(location = 3) in vec3 v_tangent;

layout(binding = 0) uniform ubo {
    mat4 prevViewProj;    
    mat4 projection;
    mat4 view;
    mat4 model;
    vec4 colour;
    vec2 jitter;
    vec2 prevJitter;
    float metallic;
    float roughness;
    uint entity;
};

layout(location = 0) out VS_OUT {
    vec2 uv;
    mat3 TBN;
    vec4 prevPos;
    vec4 currentPos;
} vs_out;

void main() {
    vs_out.currentPos = model * vec4(v_pos, 1.0);
	vs_out.currentPos = projection * view * vs_out.currentPos;
    // TODO: previous model matrix
    vs_out.prevPos = model * vec4(v_pos, 1.0);
    vs_out.prevPos = prevViewProj * vs_out.prevPos;
    
    gl_Position = vs_out.currentPos;

    vec3 N = normalize(mat3(transpose(inverse(model))) * v_normal);
	vec3 T = normalize(vec3(model * vec4(vec3(v_tangent), 0.0)));
    
    T = normalize(T - dot(T, N) * N);

	vec3 B = cross(N, T);
	vs_out.TBN = mat3(T, B, N);

	vs_out.uv = v_uv;
}