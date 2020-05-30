#version 440 core
#extension GL_ARB_shader_image_load_store : enable

layout(binding = 0) uniform sampler2D albedo;
layout(RGBA8, binding = 1) uniform image3D voxels;
layout(binding = 2) uniform sampler2DShadow shadowMap;

in vec2 uv;
in mat4 p;
in flat int axis;
in vec4 depthPosition;

void main() {
    vec4 materialColor = texture(albedo, uv);
    const int dim = imageSize(voxels).x;

    float visibility = texture(shadowMap, vec3(depthPosition.xy, (depthPosition.z - 0.001)/depthPosition.w));

	ivec3 camPos = ivec3(gl_FragCoord.x, gl_FragCoord.y, dim * gl_FragCoord.z);
	ivec3 texPos;
	if(axis == 1) {
	    texPos.x = dim - camPos.z;
		texPos.z = camPos.x;
		texPos.y = camPos.y;
	}
	else if(axis == 2) {
	    texPos.z = camPos.y;
		texPos.y = dim - camPos.z;
		texPos.x = camPos.x;
	} else {
	    texPos = camPos;
	}

	texPos.z = dim - texPos.z - 1;
        imageStore(voxels, texPos, vec4(materialColor.rgb * visibility, 1.0));
    if(visibility == 1.0) {
        //imageStore(voxels, texPos, vec4(1.0, 0.0, 0.0, 1.0));
    } else {
    }
}
