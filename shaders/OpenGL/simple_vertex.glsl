#version 330 core

layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_normal;

layout (std140) uniform stuff {
    mat4 model;
    mat4 view;
    mat4 projection;
	vec4 cameraPosition;
    vec4 DirLightPos;
	mat4 lightSpaceMatrix;
    vec4 pointLightPos;
	vec4 sunColor;
	float minBias;
	float maxBias;
} ubo;

//we send out a uv coordinate for our frag shader
out vec3 pos;
out vec3 fragPos;
out vec2 uv;
out vec3 normal;
out vec4 FragPosLightSpace;

out vec3 directionalLightPosition;
out vec3 directionalLightPositionViewSpace;
out vec3 pointLightPositionViewSpace;
out vec3 cameraPos;

void main()
{
	// stuff in view space
	pos = vec3(ubo.view * ubo.model * vec4(v_pos, 1.0));
	directionalLightPositionViewSpace = vec3(ubo.view * ubo.DirLightPos);

	fragPos = vec3(ubo.model * vec4(v_pos, 1.0));
	normal = mat3(transpose(inverse(ubo.view * ubo.model))) * v_normal;
    uv = v_uv;
	FragPosLightSpace = ubo.lightSpaceMatrix * vec4(fragPos, 1.0);
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(v_pos, 1.0);

    //////////////////////////
	pointLightPositionViewSpace = vec3(ubo.view * ubo.pointLightPos);
	directionalLightPosition = vec3(ubo.DirLightPos);
	cameraPos = vec3(ubo.cameraPosition);
}