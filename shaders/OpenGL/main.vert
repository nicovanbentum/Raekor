#version 440 core

layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_normal;
layout(location = 3) in vec3 v_tangent;
layout(location = 4) in vec3 v_binormal;

layout (std140) uniform stuff {
    mat4 model, view, projection;
	mat4 lightSpaceMatrix;
	vec4 cameraPosition;
    vec4 DirLightPos;
	vec4 pointLightPos;
} ubo;

//we send out a uv coordinate for our frag shader
out vec3 pos;
out vec3 fragPos;
out vec2 uv;
out vec3 normal;
out vec4 FragPosLightSpace;

out vec3 directionalLightPosition;
out vec3 directionalLightPositionViewSpace;
out vec3 pointLightPosition;
out vec3 pointLightPositionViewSpace;
out vec3 cameraPos;
out mat3 TBN;

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
	pointLightPosition = vec3(ubo.pointLightPos);
	pointLightPositionViewSpace = vec3(ubo.view * ubo.pointLightPos);
	directionalLightPosition = vec3(ubo.DirLightPos);
	cameraPos = vec3(ubo.cameraPosition);

	// calculate tangent space stuff
	vec3 T = normalize(vec3(ubo.view * ubo.model * vec4(v_tangent, 0.0)));
	vec3 B = normalize(vec3(ubo.view * ubo.model * vec4(v_binormal, 0.0)));
	vec3 N = normalize(vec3(ubo.view * ubo.model * vec4(v_normal, 0.0)));
	TBN = transpose(mat3(T, B, N));
}