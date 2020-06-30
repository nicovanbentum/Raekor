#version 440 core

layout(binding = 0) uniform sampler3D voxels;
uniform float voxelSize;

out vec4 color;

void main()
{
    // calculate vertices by hand
	vec3 pos; // Center of voxel
    const int dim = textureSize(voxels, 0).x;
	pos.x = gl_VertexID % dim;
	pos.z = (gl_VertexID / dim) % dim;
	pos.y = gl_VertexID / (dim * dim);

	color = texture(voxels, pos / dim);
	gl_Position = vec4(pos - dim * 0.5, 1);
}