#version 440 core

layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_normal;
layout(location = 3) in vec3 v_tangent;
layout(location = 4) in vec3 v_binormal;

layout (std140) uniform stuff {
    mat4 view, projection;
	mat4 lightSpaceMatrix;
	vec4 cameraPosition;
    vec4 DirLightPos;
	vec4 pointLightPos;
} ubo;

uniform mat4 model;

//we send out a uv coordinate for our frag shader
out vec3 PosWorldspace;
out vec2 uv;
out vec4 FragPosLightSpace;
out vec3 normal;

out vec3 directionalLightPosition;
out vec3 pointLightPosition;

out vec3 pointLightDirection;
out vec3 dirLightDirection;
out vec3 cameraDirection;
out vec3 cameraPos;

void main()
{
	// stuff in view space

	PosWorldspace = vec3(model * vec4(v_pos, 1.0));
    uv = v_uv;
	normal = normalize(v_normal);
	FragPosLightSpace = ubo.lightSpaceMatrix * vec4(PosWorldspace, 1.0);
    gl_Position = vec4(v_pos.x, v_pos.y, 0.0, 1.0); 

    //////////////////////////
	pointLightPosition = vec3(ubo.pointLightPos);
	directionalLightPosition = vec3(ubo.DirLightPos);
	cameraPos = vec3(ubo.cameraPosition);

	// calculate tangent space stuff
	vec3 T = normalize(vec3(ubo.view * model * vec4(v_tangent, 0.0)));
	vec3 B = normalize(vec3(ubo.view * model * vec4(v_binormal, 0.0)));
	vec3 N = normalize(vec3(ubo.view * model * vec4(v_normal, 0.0)));
	mat3 TBN = transpose(mat3(T, B, N));

	#ifdef NO_NORMAL_MAP
	TBN = mat3(1.0f);
	#endif
}