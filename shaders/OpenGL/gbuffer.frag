#version 440 core

// TODO: optimize by packing more data per byte
// MRT texture output 
// NOTE: always render to 1, 2 or 4 component vectors
layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gColor;

// constant mesh values
layout(binding = 0) uniform sampler2D meshTexture;
layout(binding = 3) uniform sampler2D normalMap;

in vec3 pos;
in vec2 uv;
in mat3 TBN;

void main() {
	// write the color to the color texture of the gbuffer
	gColor = texture(meshTexture, uv);

	// retrieve the normal from the normal map
    vec4 sampledNormal = texture(normalMap, uv);
	gNormal = normalize(sampledNormal * 2.0 - 1.0);
	gNormal = vec4(normalize(TBN * gNormal.xyz), 1.0);

    // TODO: why the frick can I not use sampledNormal directly??
    if(sampledNormal.x == 0.0 && sampledNormal.y == 0.0 && sampledNormal.z == 1.0) {
        gNormal = vec4(normalize(TBN * vec3(0, 0, 1)), 1.0);
    }

	// positional data comes in from the vertex shader
	gPosition = vec4(pos, 1.0);
}