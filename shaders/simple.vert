#version 330 core


layout(location = 0) in vec3 vertex_pos;
layout(location = 1) in vec2 vertex_uv;

//we send out a uv coordinate for our frag shader
out vec2 uv;

//4x4 matrix for our model view camera
uniform mat4 MVP;

void main()
{
    //update the position of the vertex in space
    gl_Position = MVP * vec4(vertex_pos, 1);

    // set output uv
    uv = vertex_uv;
}