#version 440 core

layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_normal;
layout(location = 3) in vec3 v_tangent;
layout(location = 4) in vec3 v_bitangent;

#define MAX_POINT_LIGHTS 10
#define MAX_DIR_LIGHTS 1

struct DirectionalLight {
    vec3 direction;
	vec4 color;
};

struct PointLight {
	vec4 position;
	vec4 color;
};

layout (binding = 0, std140) uniform Uniforms {
    mat4 model;
	mat4 view, projection;
	mat4 lightSpaceMatrix;
	vec4 cameraPosition;
    DirectionalLight dirLights[MAX_DIR_LIGHTS];
    PointLight pointLights[MAX_POINT_LIGHTS];
};

layout(location = 0) out VS_OUT {
    vec2 uv;
    vec3 position;
    vec3 normal;
    mat3 TBN;
    vec3 cameraDirection;
    vec4 depthPosition;
} vs_out;


void main() {
	vs_out.position = (model * vec4(v_pos ,1)).xyz;
    gl_Position =  projection * view * vec4(vs_out.position , 1.0);

    vec3 T = normalize(vec3(model * vec4(v_tangent,		0.0)));
	vec3 B = normalize(vec3(model * vec4(v_bitangent,	0.0)));
	vs_out.normal = normalize(vec3(model * vec4(v_normal, 0.0)));
	vs_out.TBN = mat3(T, B, vs_out.normal.xyz);

    vs_out.depthPosition = lightSpaceMatrix * model * vec4(v_pos, 1);
	vs_out.depthPosition.xyz = vs_out.depthPosition.xyz * 0.5 + 0.5;

	vs_out.cameraDirection = cameraPosition.xyz - vs_out.position;

	vs_out.uv = v_uv;
}