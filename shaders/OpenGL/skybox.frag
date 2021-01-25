#version 450

layout(location = 0) out vec4 final_color;

layout(location = 0) in vec3 texturePos;

layout(binding = 0) uniform samplerCube skybox;

void main()
{    
    final_color = vec4(textureLod(skybox, texturePos, 1.2).rgb, 1.0);
	//color = vec4(texture_pos.xyz, 1.0f); // debug colors
}