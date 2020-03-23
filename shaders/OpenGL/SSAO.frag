#version 440 core

out vec4 FragColor;

in vec2 uv;


layout(binding = 0) uniform sampler2D gPosition;
layout(binding = 1) uniform sampler2D gNormals;
layout(binding = 2) uniform sampler2D noiseTexture;

uniform vec3 samples[64];
uniform mat4 projection;
uniform mat4 view;
uniform vec2 noiseScale;

uniform float power;
uniform float bias;

void main() {
		
	// get view space vectors
	vec3 position = vec3(view * vec4(texture(gPosition, uv).xyz, 1.0));
	mat3 normalMatrix = transpose(inverse(mat3(view)));
	vec3 normal = normalMatrix * texture(gNormals, uv).xyz;

	// create TBN matrix that goes from tangent to view space
	vec3 random = texture(noiseTexture, uv * noiseScale).xyz;
	vec3 tangent = normalize(random - normal * dot(random, normal));
	vec3 bitangent = cross(normal, tangent);
	mat3 TBN = mat3(tangent, bitangent, normal);

	float occlusion = 0.0;
	for(int i = 0; i < 64; i++) {
	// convert the random vector from tangent to view space
		vec3 sampled = TBN * samples[i];
		sampled = position + sampled * 0.5;

		// convert the offset from world to NDC
		vec4 offset = vec4(sampled, 1.0);
		offset = projection * offset;
		offset.xyz /= offset.w;
		offset.xyz = offset.xyz * 0.5 + 0.5;

		// get the sampled depth in world space and convert it to view space
		float sampledDepth = vec3(view * vec4(texture(gPosition, offset.xy).xyz, 1.0)).z;
		float rangecheck = smoothstep(0.0, 1.0, 0.5 / abs(position.z - sampledDepth));
		occlusion += (sampledDepth >= sampled.z + bias ? 1.0 : 0.0) * rangecheck;
	}

	occlusion = 1.0 - (occlusion / 64);
	occlusion = pow(occlusion, power);
	FragColor = vec4(occlusion, occlusion, occlusion, 1.0);
}