#version 330 core


layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 1) in vec2 vertexUV;

//we send out a rgb color for our fragment shader
out vec2 UV;

//4x4 matrix for our model view camera
uniform mat4 MVP;

void main()
{
    //update the position of the vertex in space
    gl_Position = MVP * vec4(vertexPosition_modelspace, 1);

    //set vertex color
    UV = vertexUV;
}