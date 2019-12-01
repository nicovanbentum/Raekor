#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_vulkan_glsl : enable

// in vars
layout(location = 0) in vec4 color;
layout(location = 1) in vec2 uv;
// normal, direction and light direction in camera space
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 direction;
layout(location = 4) in vec3 light_direction;
// gl position in world space
layout(location = 5) in vec3 position;
// light position in world space
layout(location = 6) in vec3 light_pos;

// in uniforms
layout(set = 0, binding = 1) uniform sampler2D tex_sampler[24]; 
layout(push_constant) uniform pushConstants {
	int samplerIndex;
} pc;

// out vars
layout(location = 0) out vec4 final_color;

void main() {
	vec3 light_color = vec3(1, 1, 1);
	float light_power = 50.0f;

    // Material properties
	vec3 diffuse_color = texture( tex_sampler[pc.samplerIndex], uv ).rgb;
	vec3 ambient_color = vec3(0.1, 0.1, 0.1) * diffuse_color;
	vec3 specular_color = vec3(0.3,0.3,0.3);

    // Distance to the light
	float distance = length(light_pos - position);

	// Normal of the computed fragment, in camera space
	vec3 n = normalize(normal);
	// Direction of the light (from the fragment to the light)
	vec3 l = normalize(light_direction);
	// Cosine of the angle between the normal and the light direction, 
	// clamped above 0
	//  - light is at the vertical of the triangle -> 1
	//  - light is perpendicular to the triangle -> 0
	//  - light is behind the triangle -> 0
	float cosTheta = clamp( dot( n,l ), 0,1 );
	
	// Eye vector (towards the camera)
	vec3 E = normalize(direction);
	// Direction in which the triangle reflects the light
	vec3 R = reflect(-l,n);
	// Cosine of the angle between the Eye vector and the Reflect vector,
	// clamped to 0
	//  - Looking into the reflection -> 1
	//  - Looking elsewhere -> < 1
	float cosAlpha = clamp( dot( E,R ), 0,1 );
    vec3 fc = 
		// Ambient : simulates indirect lighting
		ambient_color +
		// Diffuse : "color" of the object
		diffuse_color * light_color * light_power * cosTheta / (distance*distance) +
		// Specular : reflective highlight, like a mirror
		specular_color * light_color * light_power * pow(cosAlpha,5) / (distance*distance);
	
	final_color = vec4(fc, texture(tex_sampler[pc.samplerIndex], uv).a);
}