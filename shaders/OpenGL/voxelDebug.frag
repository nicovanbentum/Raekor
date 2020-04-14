#version 440 core

#define INV_STEP_LENGTH (1.0f/STEP_LENGTH)
#define STEP_LENGTH 0.005f

out vec4 final_color;

in vec2 TexCoords;

layout(binding = 0) uniform sampler2D backTexture;
layout(binding = 1) uniform sampler2D frontTexture;
layout(binding = 2) uniform sampler3D voxelTexture;

uniform vec3 cameraPosition;

// Scales and bias a given vector (i.e. from [-1, 1] to [0, 1]).
vec3 scaleAndBias(vec3 p) { return 0.5f * p + vec3(0.5f); }

// Returns true if p is inside the unity cube (+ e) centered on (0, 0, 0).
bool isInsideCube(vec3 p, float e) { return abs(p.x) < 1 + e && abs(p.y) < 1 + e && abs(p.z) < 1 + e; }

void main() {
// Initialize ray.
	const vec3 origin = isInsideCube(cameraPosition, 0.2f) ? 
		cameraPosition : texture(frontTexture, TexCoords).xyz;
	vec3 direction = texture(backTexture, TexCoords).xyz - origin;
	const uint numberOfSteps = uint(INV_STEP_LENGTH * length(direction));
	direction = normalize(direction);

	// Trace.
	final_color = vec4(0.0f);
	for(uint step = 0; step < numberOfSteps && final_color.a < 0.99f; ++step) {
		const vec3 currentPoint = origin + STEP_LENGTH * step * direction;
		vec3 coordinate = scaleAndBias(currentPoint);
		vec4 currentSample = textureLod(voxelTexture, scaleAndBias(currentPoint), 0);
		final_color += (1.0f - final_color.a) * currentSample;
	} 
	//final_color.a = 1.0f;
	//final_color.rgb = pow(final_color.rgb, vec3(1.0 / 2.2));
}