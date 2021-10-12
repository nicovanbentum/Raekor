#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) out vec4 color;

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec4 in_col;


layout(push_constant) uniform pushConstants {
    mat4 proj;
    int textureIndex;
};

void main() {
    vec4 texColor = vec4(1.0);

    if(textureIndex > -1) {
        texColor = texture(textures[textureIndex], in_uv);  
    }

    texColor.a    = smoothstep(0.0f, 1.0f, texColor.r);
    texColor  = vec4(1.0f, 1.0f, 1.0f, texColor.a);

    color = in_col * texColor;
}