#version 440 core
#extension GL_ARB_shader_image_load_store : enable

layout(binding = 0) uniform sampler2D albedo;
layout(rgba8, binding = 1) uniform image3D voxels;
layout(binding = 2) uniform sampler2DShadow shadowMap;

in vec2 uv;
in mat4 p;
in flat int axis;
in vec4 depthPosition;

void main() {
    vec4 sampled = texture(albedo, uv);
    const int dim = imageSize(voxels).x;

    // TODO: uniform bias and stuff
    float shadowAmount = texture(shadowMap, vec3(depthPosition.xy, (depthPosition.z - 0.001)/depthPosition.w));

	ivec3 camPos = ivec3(gl_FragCoord.x, gl_FragCoord.y, dim * gl_FragCoord.z);
	ivec3 voxelPosition;
	if(axis == 1) {
	    voxelPosition.x = dim - camPos.z;
		voxelPosition.z = camPos.x;
		voxelPosition.y = camPos.y;
	}
	else if(axis == 2) {
	    voxelPosition.z = camPos.y;
		voxelPosition.y = dim - camPos.z;
		voxelPosition.x = camPos.x;
	} else {
	    voxelPosition = camPos;
	}

	voxelPosition.z = dim - voxelPosition.z - 1;
    imageStore(voxels, voxelPosition, vec4(sampled.rgb * shadowAmount, sampled.a));
}
