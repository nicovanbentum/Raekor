#version 450 core
layout (location = 0) in vec3 pos;

layout(location = 0) out vec3 texturePos;

layout(binding = 0) uniform ubo {
    mat4 view;
    mat4 proj;
    mat4 invViewProj;
    vec4 cameraPos;
    vec4 sunlightDir;
    vec4 sunlightColor;
};

void main() {
      vec4 glPos = proj * view * vec4(pos, 1.0);
	  gl_Position = glPos.xyww;
      texturePos = pos;
}  