#version 460 core

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
    float mLODFade;
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

layout(location = 0) out VS_OUT {
    vec2 uv;
    mat3 TBN;
    vec4 prevPos;
    vec4 currentPos;
} vs_out;

void main() {
    Vertex v = vertices[gl_VertexID];
    vec3 v_pos = vec3(v.pos[0], v.pos[1], v.pos[2]);
    vec3 v_normal = vec3(v.normal[0], v.normal[1], v.normal[2]);
    vec3 v_tangent = vec3(v.tangent[0], v.tangent[1], v.tangent[2]);

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

	vs_out.uv = vec2(v.uv[0], v.uv[1]);
}