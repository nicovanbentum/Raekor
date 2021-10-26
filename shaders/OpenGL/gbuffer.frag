#version 460 core
#extension GL_GOOGLE_include_directive : require

#include "include/structs.glsl"

//#extension GL_ARB_bindless_texture : require

// TODO: optimize by packing more data per byte
// MRT texture output 
// NOTE: always render to 1, 2 or 4 component vectors
layout(location = 0) out vec4 gNormal;
layout(location = 1) out vec4 gColor;
layout(location = 2) out vec4 gMetallicRoughness;
layout(location = 3) out vec4 gEntityID;
layout(location = 4) out vec4 gVelocity;

// constant mesh values
layout(binding = 1) uniform sampler2D albedoTexture;
layout(binding = 2) uniform sampler2D normalTexture;
layout(binding = 3) uniform sampler2D metalroughTexture;

layout(location = 0) in VS_OUT {
    vec2 uv;
    mat3 TBN;
    vec4 prevPos;
    vec4 currentPos;
} vs_out;

layout(binding = 0) uniform ubo {
    mat4 prevViewProj;
    mat4 projection;
    mat4 view;
    mat4 model;
    vec4 colour;
    vec2 jitter;
    vec2 prevJitter;
    float metallic;
    float roughness;
    uint entity;
};

void main() {
    vec4 color = texture(albedoTexture, vs_out.uv);
    if(color.a < 0.5) discard;
	// write the color to the color texture of the gbuffer
	gColor = color * colour;

	// retrieve the normal from the normal map
    vec4 sampledNormal = texture(normalTexture, vs_out.uv);
	vec3 glNormal = sampledNormal.xyz * 2.0 - 1.0;
    vec3 normal = vs_out.TBN * glNormal;
	gNormal = vec4(normal, 1.0);

    vec4 metalrough = texture(metalroughTexture, vs_out.uv);
    gMetallicRoughness = vec4(metalrough.r, metalrough.g * roughness, metalrough.b * metallic, 1.0);

    vec3 prevPosNDC = (vs_out.prevPos.xyz / vs_out.prevPos.w);
    vec3 currentPosNDC = (vs_out.currentPos.xyz / vs_out.currentPos.w);
    
    gVelocity = vec4(currentPosNDC.xy - prevPosNDC.xy, 0.0, 1.0);

    gEntityID = vec4(entity, 0, 0, 1.0);
}