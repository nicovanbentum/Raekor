#version 440 core

#define INV_STEP_LENGTH (1.0f/STEP_LENGTH)
#define STEP_LENGTH 0.005f

out vec4 finalColor;

in vec2 uv;

layout(binding = 0) uniform sampler2D backTexture;
layout(binding = 1) uniform sampler2D frontTexture;
layout(binding = 2) uniform sampler3D voxelTexture;

uniform vec3 cameraPosition;

// Scales and bias a given vector (i.e. from [-1, 1] to [0, 1]).
vec3 scaleAndBias(vec3 p) { return 0.5f * p + vec3(0.5f); }

// Returns true if p is inside the unit cube (+ e) centered on (0, 0, 0).
bool isInsideCube(vec3 p, float e) { return abs(p.x) < 1 + e && abs(p.y) < 1 + e && abs(p.z) < 1 + e; }

void main() {
    vec4 frontSampled = texture(frontTexture, uv);
    vec4 backSampled = texture(backTexture, uv);

    vec3 pos = frontSampled.xyz;
    vec4 dst = vec4(0.0, 0.0, 0.0, 0.0);


    // Initialize ray.
    vec3 origin = frontSampled.xyz;
	vec3 direction = backSampled.xyz - origin;
	const uint numberOfSteps = uint(length(direction) / STEP_LENGTH);
	direction = normalize(direction);

	// Trace.
	finalColor = vec4(0.0f);
	for(uint i = 0; i < numberOfSteps && finalColor.a < 0.99f; ++i) {
		const vec3 currentPoint = origin + STEP_LENGTH * i * direction;
		vec4 currentSample = textureLod(voxelTexture, scaleAndBias(currentPoint), 0);
		finalColor += (1.0f - finalColor.a) * currentSample;
	} 
	//finalColor.a = 1.0f;
	//finalColor.rgb = pow(final_color.rgb, vec3(1.0 / 2.2));
}