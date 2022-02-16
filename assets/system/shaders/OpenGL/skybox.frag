#version 450

layout(location = 0) out vec4 final_color;

layout(location = 0) in vec3 texturePos;

layout(binding = 0) uniform ubo {
    mat4 view;
    mat4 proj;
    mat4 invViewProj;
    vec4 cameraPos;
    vec4 sunlightDir;
    vec4 sunlightColor;
};

layout(binding = 1) uniform samplerCube skybox;


void main() {
    vec3 uv = texturePos;
    final_color = vec4(texture(skybox, texturePos).rgb, 1.0);
	//final_color = vec4(texturePos.xyz, 1.0f); // debug colors
}