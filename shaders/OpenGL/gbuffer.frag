#version 460 core

// TODO: optimize by packing more data per byte
// MRT texture output 
// NOTE: always render to 1, 2 or 4 component vectors
layout(location = 0) out vec4 gNormal;
layout(location = 1) out vec4 gColor;
layout(location = 2) out vec4 gMetallicRoughness;
layout(location = 3) out vec4 gEntityID;

// constant mesh values
layout(binding = 1) uniform sampler2D albedoTexture;
layout(binding = 2) uniform sampler2D normalTexture;
layout(binding = 3) uniform sampler2D metalroughTexture;

layout(binding = 0) uniform ubo {
      mat4 projection;
      mat4 view;
      mat4 model;
      vec4 colour;
      uint entity;
};

in vec2 uv;
in mat3 TBN;

void main() {
    vec4 color = texture(albedoTexture, uv);
    if(color.a < 0.5) discard;
	// write the color to the color texture of the gbuffer
	gColor = color * colour;

	// retrieve the normal from the normal map
    vec4 sampledNormal = texture(normalTexture, uv);
	vec3 glNormal = sampledNormal.xyz * 2.0 - 1.0;
    vec3 normal = TBN * glNormal;
	gNormal = vec4(normal, 1.0);

    vec4 metalrough = texture(metalroughTexture, uv);
    gMetallicRoughness = vec4(metalrough.r, metalrough.g, metalrough.b, 1.0);
    gEntityID = vec4(entity, 0, 0, 1.0);
}