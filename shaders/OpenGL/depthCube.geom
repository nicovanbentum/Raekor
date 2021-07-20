#version 440 core
layout (triangles) in;
layout (triangle_strip, max_vertices=18) out;

layout (binding = 0, std140) uniform Uniforms {
    mat4 model;
	mat4 faceMatrices[6];
	vec4 lightPos;
    mat4 projView;
	float farPlane;
	float x, y, z;
};

layout(location = 0) out vec4 FragPos;

void main()
{
    for(int face = 0; face < 6; ++face) {
        gl_Layer = face;
        for(int i = 0; i < 3; ++i) {
            FragPos = gl_in[i].gl_Position;
            gl_Position = faceMatrices[face] * FragPos;
            EmitVertex();
        }    
        EndPrimitive();
    }
}  