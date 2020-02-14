#version 440 core
layout (triangles) in;
layout (triangle_strip, max_vertices=18) out;

layout (std140) uniform stuff {
    mat4 model;
	mat4 faceMatrices[6];
	vec4 lightPos;
	float farPlane;
	float x, y, z;
} ubo;

out vec4 FragPos;

void main()
{
    for(int face = 0; face < 6; ++face) {
        gl_Layer = face;
        for(int i = 0; i < 3; ++i) {
            FragPos = gl_in[i].gl_Position;
            gl_Position = ubo.faceMatrices[face] * FragPos;
            EmitVertex();
        }    
        EndPrimitive();
    }
}  