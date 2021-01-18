#version 450

layout(location = 0) out vec4 albedo;
layout(location = 1) out vec4 entity_id;

layout(location = 0) in vec2 uv;

layout(binding = 0) uniform sampler2D uTexture;

uniform uint entity;

void main() {
    vec4 sampled = texture(uTexture, uv);
    if(sampled.a < 1.0) {
        discard;
    }
    albedo = texture(uTexture, uv);
    entity_id = vec4(entity, 0.0, 0.0, 1.0);
}