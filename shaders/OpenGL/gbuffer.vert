#version 460 core

// vertex buffer data
layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_normal;
layout(location = 3) in vec3 v_tangent;

layout(binding = 0) uniform ubo {
      mat4 projection;
      mat4 view;
      mat4 model;
      vec4 colour;
      float metallic;
      float roughness;
      uint entity;
};

layout(location = 0) out VS_OUT {
    vec2 uv;
    mat3 TBN;
} vs_out;

void main() {
	vec3 pos = vec3(model * vec4(v_pos, 1.0));
	gl_Position = projection * view * vec4(pos, 1.0);

    vec3 N = normalize(mat3(transpose(inverse(model))) * v_normal);
	vec3 T = normalize(vec3(model * vec4(vec3(v_tangent), 0.0)));
    
    T = normalize(T - dot(T, N) * N);

	vec3 B = cross(N, T);
	vs_out.TBN = mat3(T, B, N);

	vs_out.uv = v_uv;
}