#version 440 core

layout(binding = 0) uniform sampler3D voxels;

layout(location = 0) out vec4 color;
layout(location = 1) out uint instance;

uniform mat4 p;
uniform mat4 mv;
uniform vec3 cameraPosition;

vec4 center_to_edge[8] = {
	vec4(-0.5, 0.5, 0.5, 0.0), 
	vec4(0.5, 0.5, 0.5, 0.0),
	vec4(-0.5, 0.5, -0.5, 0.0),
	vec4(0.5, 0.5, -0.5, 0.0),
	vec4(-0.5, -0.5, 0.5, 0.0),
	vec4(0.5, -0.5, 0.5, 0.0),
	vec4(-0.5, -0.5, -0.5, 0.0),
	vec4(0.5, -0.5, -0.5, 0.0)
	};

void main() {
	uint vx = gl_VertexID;
	instance = vx >> 3;

	vec3 instance_pos;
	vec3 local_camera_pos = cameraPosition - instance_pos;

	uvec3 xyz = uvec3(vx & 0x1, (vx & 0x4) >> 2, (vx & 0x2) >> 1);
	
	if(local_camera_pos.x > 0) xyz.x = 1 - xyz.x;
	if(local_camera_pos.y > 0) xyz.y = 1 - xyz.y;
	if(local_camera_pos.z > 0) xyz.z = 1 - xyz.z;

	vec3 uvw = vec3(xyz);
	vec3 pos = uvw * 2.0 - 1.0;

    // calculate vertices by hand
	vec3 texPos; // 3d texture coordinate, so like, (64, 39, 21)
    const int dim = textureSize(voxels, 0).x;
	texPos.x = instance % dim;
	texPos.z = (instance / dim) % dim;
	texPos.y = instance / (dim * dim);

	color = texture(voxels, texPos / dim); // 3d texture coordinate in 0-1 range
	vec4 glPos = vec4(texPos - dim * 0.5, 1);

	gl_Position = p * mv * (glPos + center_to_edge[vx % 8]);
}