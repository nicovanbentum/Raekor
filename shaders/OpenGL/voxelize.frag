#version 450
#extension GL_ARB_shader_image_load_store : require

layout(binding = 0) uniform sampler2D albedo;
layout(r32ui, binding = 1) uniform volatile coherent uimage3D voxels;

layout(binding = 2) uniform sampler2D rtShadows;

in vec2 uv;
in flat int axis;

// SOURCE: https://www.seas.upenn.edu/~pcozzi/OpenGLInsights/OpenGLInsights-SparseVoxelization.pdf
vec4 convRGBA8ToVec4(uint val)
{
    return vec4(float((val & 0x000000FF)), 
    float((val & 0x0000FF00) >> 8U), 
    float((val & 0x00FF0000) >> 16U), 
    float((val & 0xFF000000) >> 24U));
}

uint convVec4ToRGBA8(vec4 val)
{
    return (uint(val.w) & 0x000000FF) << 24U | 
    (uint(val.z) & 0x000000FF) << 16U | 
    (uint(val.y) & 0x000000FF) << 8U | 
    (uint(val.x) & 0x000000FF);
}

void imageAtomicRGBA8Avg(ivec3 coords, vec4 value) {
    value.rgb *= 255.0;
    uint newVal = convVec4ToRGBA8(value);
    uint prevStoredVal = 0;
    uint curStoredVal;

    while(true) {
        // if curstoredval = prevstoredval, write newval
        curStoredVal = imageAtomicCompSwap(voxels, coords, prevStoredVal, newVal);
        if(curStoredVal == prevStoredVal) break;
    
        prevStoredVal = curStoredVal;
        vec4 rval = convRGBA8ToVec4(curStoredVal);
        rval.rgb = (rval.rgb * rval.a); // Denormalize
        vec4 curValF = rval + value;    // Add
        curValF.rgb /= curValF.a;      // Renormalize
        newVal = convVec4ToRGBA8(curValF);
    }
}

void main() {
    vec4 sampled = texture(albedo, uv);
    const int dim = imageSize(voxels).x;

    // TODO: improve shadow sampling
    float shadowAmount = texture(rtShadows, uv).r;

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
    imageAtomicRGBA8Avg(voxelPosition, writeVal);
}
