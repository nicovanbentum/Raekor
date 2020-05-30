#version 440 core

layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_normal;

uniform mat4 model;
uniform mat4 lightViewProjection;

out vec2 uvs;
out vec4 depthPositions;

void main() {

    gl_Position = model * vec4(v_pos ,1);

    depthPositions = lightViewProjection * model * vec4(v_pos, 1);
	depthPositions.xyz = depthPositions.xyz * 0.5 + 0.5;

    uvs = v_uv;
}