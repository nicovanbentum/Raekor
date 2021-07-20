#version 450
#extension GL_ARB_shader_image_load_store : require
// #extension GL_ARB_fragment_shader_interlock : require
// layout(pixel_interlock_ordered) in;

layout(binding = 0) uniform sampler2D albedo;
layout(rgba8, binding = 1) uniform coherent image3D voxels;

layout(binding = 2) uniform sampler2DArrayShadow shadowMap;

layout(binding = 0) uniform ubo {
    mat4 px, py, pz;
    mat4 shadowMatrices[4];
    mat4 view;
    mat4 model;
    vec4 shadowSplits;
    vec4 colour;
};

layout(location = 0) in GEOM_OUT {
    vec2 uv;
    flat int axis;
    vec4 worldPosition;
    vec4 normal;
};


void main() {
    vec4 sampled = texture(albedo, uv) * colour;
    if(sampled.a < 0.5) discard;
    const int dim = imageSize(voxels).x;

    uint cascadeIndex = 0;
	for(uint i = 0; i < 4 - 1; i++) {
		if((view * worldPosition).z < shadowSplits[i]) {
			cascadeIndex = i + 1;
		}
	}

    vec4 depthPosition = shadowMatrices[cascadeIndex] * worldPosition;
    depthPosition.xyz = depthPosition.xyz * 0.5 + 0.5;

    float shadowAmount = texture(shadowMap, vec4(depthPosition.xy, cascadeIndex, (depthPosition.z)/depthPosition.w));

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
    vec4 writeVal = vec4(sampled.rgb * shadowAmount, sampled.a);

    //beginInvocationInterlockARB();

    memoryBarrier();
    vec4 curVal = imageLoad(voxels, voxelPosition);

    float Ao = curVal.a + writeVal.a * (1 - curVal.a);
    vec3 Co = curVal.rgb * curVal.a + writeVal.rgb * writeVal.a * (1 - curVal.a);
    Co = Co / Ao;

    imageStore(voxels, voxelPosition, vec4(Co, sampled.a));

    //endInvocationInterlockARB();

}
