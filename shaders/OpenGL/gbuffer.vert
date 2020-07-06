#version 440 core

// vertex buffer data
layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_normal;
layout(location = 3) in vec3 v_tangent;
layout(location = 4) in vec3 v_binormal;
layout(location = 5) in vec4 boneIndices;
layout(location = 6) in vec4 boneWeights;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

uniform mat4 boneTransforms[100];

out vec3 pos;
out vec2 uv;
out mat3 TBN;

void main() {
    mat4 boneTransform = boneTransforms[int(boneIndices[0])] * boneWeights[0];
    boneTransform += boneTransforms[int(boneIndices[1])] * boneWeights[1];
    boneTransform += boneTransforms[int(boneIndices[2])] * boneWeights[2];
    boneTransform += boneTransforms[int(boneIndices[3])] * boneWeights[3];

	pos = vec3(boneTransform * model * vec4(v_pos, 1.0));
	gl_Position = projection * view * vec4(pos, 1.0);

	vec3 T = normalize(vec3(boneTransform * model * vec4(v_tangent,		0.0)));
	vec3 B = normalize(vec3(boneTransform * model * vec4(v_binormal,	0.0)));
	vec3 N = normalize(vec3(boneTransform * model * vec4(v_normal, 0.0)));
	TBN = mat3(T, B, N);

	uv = v_uv;
}