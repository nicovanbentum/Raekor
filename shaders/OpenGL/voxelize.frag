#version 440 core

layout(binding = 0) uniform sampler2D albedo;
layout(rgba32f, binding = 1) uniform image3D voxels;

in vec3 f_position;
in vec3 f_normal;
in vec2 f_uv;

vec3 scaleAndBias(vec3 p) { return 0.5f * p + vec3(0.5f); }

bool isInsideCube(const vec3 p, float e) { return abs(p.x) < 1 + e && abs(p.y) < 1 + e && abs(p.z) < 1 + e; }

void main() {
	if(!isInsideCube(f_position, 0)) {
		//return;
	}

	vec4 sampled = texture(albedo, f_uv);
	vec3 voxel = scaleAndBias(f_position);
	ivec3 dim = imageSize(voxels);
	vec4 res = vec4(sampled.xyz, 1);
	// debug test by filling the 3d texture with red pixel data
	imageStore(voxels, ivec3(dim * voxel), res);
}
