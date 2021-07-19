#version 450

layout(location = 0) out vec2 uv;

layout(binding = 0) uniform ubo {
   mat4 mvp;
   vec4 world_position;
   uint entity;
};

void main() 
{
    float x = float(((uint(gl_VertexID) + 2u) / 3u)%2u); 
    float y = float(((uint(gl_VertexID) + 1u) / 3u)%2u); 

    gl_Position = vec4(-1.0f + x*2.0f, -1.0f+y*2.0f, 0.0f, 1.0f);
    uv = vec2(x, y);

    gl_Position = mvp * gl_Position;
}