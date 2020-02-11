#version 330 core

layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_normal;

layout (std140) uniform Camera {
    mat4 model;
    mat4 view;
    mat4 projection;
    vec4 lightPos;
    vec4 lightAngle;
	mat4 lightSpaceMatrix;
};

//we send out a uv coordinate for our frag shader
out vec3 fragPos;
out vec4 color;
out vec2 uv;
out vec3 normal;
out vec3 pos;
out vec3 light_pos;
out vec3 light_angle;
out vec4 pos_light_space;

void main()
{
	fragPos = vec3(model * vec4(v_pos, 1.0));
	pos_light_space = lightSpaceMatrix * vec4(fragPos, 1.0);

	// CAMERA SPACE : VERTEX POSITION
    gl_Position = projection * view * model * vec4(v_pos, 1.0);
	pos = vec3(view * model * vec4(v_pos, 1.0));
	normal = mat3(transpose(inverse(view * model))) * v_normal;
	light_pos = vec3(view * lightPos);
	light_angle = vec3(view * lightAngle);

    // set output uv
    uv = v_uv;
}