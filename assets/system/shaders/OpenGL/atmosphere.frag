#version 450

// -------------------------------------
// Defines
#define EPS                 1e-6
#define PI                  3.14159265359
#define INFINITY            1.0 / 0.0
#define PLANET_RADIUS       637100
#define PLANET_CENTER       vec3(0, PLANET_RADIUS, 0)
#define ATMOSPHERE_HEIGHT   500000
#define RAYLEIGH_HEIGHT     (ATMOSPHERE_HEIGHT * 0.08)
#define MIE_HEIGHT          (ATMOSPHERE_HEIGHT * 0.012)

// #define EPS                 1e-6
// #define PI                  3.14159265359
// #define INFINITY            1.0 / 0.0
// #define PLANET_RADIUS       637100
// #define PLANET_CENTER       vec3(0, PLANET_RADIUS, 0)
// #define ATMOSPHERE_HEIGHT   500000
// #define RAYLEIGH_HEIGHT     (ATMOSPHERE_HEIGHT * 0.08)
// #define MIE_HEIGHT          (ATMOSPHERE_HEIGHT * 0.012)

// -------------------------------------
// Coefficients
#define C_RAYLEIGH          (vec3(5.802, 13.558, 33.100) * EPS)
#define C_MIE               (vec3(3.996,  3.996,  3.996) * EPS)
#define C_OZONE             (vec3(0.650,  1.881,  0.085) * EPS)

#define ATMOSPHERE_DENSITY  .25
#define EXPOSURE            3

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 uv;

layout(binding = 0) uniform ubo {
    mat4 invViewProj;
    vec4 cameraPos;
    vec4 sunlightDir;
    vec4 sunlightColor;
};

vec2 SphereIntersection(vec3 rayStart, vec3 rayDir, vec3 sphereCenter, float sphereRadius) {
    rayStart -= sphereCenter;
	float a = dot(rayDir, rayDir);
	float b = 2.0 * dot(rayStart, rayDir);
	float c = dot(rayStart, rayStart) - (sphereRadius * sphereRadius);
	float d = b * b - 4 * a * c;
	if (d < 0)
	{
		return vec2(-1, 0);
	}
	else
	{
		d = sqrt(d);
		return vec2(-b - d, -b + d) / (2 * a);
	}
}

vec2 PlanetIntersection (vec3 rayStart, vec3 rayDir) {
	return SphereIntersection(rayStart, rayDir, PLANET_CENTER, PLANET_RADIUS);
}
vec2 AtmosphereIntersection (vec3 rayStart, vec3 rayDir) {
	return SphereIntersection(rayStart, rayDir, PLANET_CENTER, PLANET_RADIUS + ATMOSPHERE_HEIGHT);
}

// -------------------------------------
// Phase functions
float PhaseRayleigh (float costh) {
	return 3 * (1 + costh*costh) / (16 * PI);
}

float PhaseMie (float costh, float g) {
	g = min(g, 0.9381);
	float k = 1.55*g - 0.55*g*g*g;
	float kcosth = k*costh;
	return (1 - k*k) / ((4 * PI) * (1-kcosth) * (1-kcosth));
}

float PhaseMie(float costh) {
    return PhaseMie(costh, 0.85);
}

// -------------------------------------
// Atmosphere
float AtmosphereHeight(vec3 pos) {
    return distance(pos, PLANET_CENTER) - PLANET_RADIUS;
}

float DensityRayleigh (float h)
{
	return exp(-max(0, h / RAYLEIGH_HEIGHT));
}
float DensityMie (float h)
{
	return exp(-max(0, h / MIE_HEIGHT));
}
float DensityOzone (float h)
{
	// The ozone layer is represented as a tent function with a width of 30km, centered around an altitude of 25km.
	return max(0, 1 - abs(h - 25000.0) / 15000.0);
}

vec3 AtmosphereDensity (float h)
{
	return vec3(DensityRayleigh(h), DensityMie(h), DensityOzone(h));
}

// Optical depth is a unitless measurement of the amount of absorption of a participating medium (such as the atmosphere).
// This function calculates just that for our three atmospheric elements:
// R: Rayleigh
// G: Mie
// B: Ozone
// If you find the term "optical depth" confusing, you can think of it as "how much density was found along the ray in total".
vec3 IntegrateOpticalDepth (vec3 rayStart, vec3 rayDir)
{
	vec2 intersection = AtmosphereIntersection(rayStart, rayDir);
	float  rayLength    = intersection.y;

	int    sampleCount  = 8;
	float  stepSize     = rayLength / sampleCount;
	
	vec3 opticalDepth = vec3(0);

	for (int i = 0; i < sampleCount; i++)
	{
		vec3 localPosition = rayStart + rayDir * (i + 0.5) * stepSize;
		float  localHeight   = AtmosphereHeight(localPosition);
		vec3 localDensity  = AtmosphereDensity(localHeight);

		opticalDepth += localDensity * stepSize;
	}

	return opticalDepth;
}

// Calculate a luminance transmittance value from optical depth.
vec3 Absorb (vec3 opticalDepth)
{
	// Note that Mie results in slightly more light absorption than scattering, about 10%
	return exp(-(opticalDepth.x * C_RAYLEIGH + opticalDepth.y * C_MIE * 1.1 + opticalDepth.z * C_OZONE) * ATMOSPHERE_DENSITY);
}

// Integrate scattering over a ray for a single directional light source.
// Also return the transmittance for the same ray as we are already calculating the optical depth anyway.
vec3 IntegrateScattering (vec3 rayStart, vec3 rayDir, float rayLength, vec3 lightDir, vec3 lightColor, out vec3 transmittance)
{
	// We can reduce the number of atmospheric samples required to converge by spacing them exponentially closer to the camera.
	// This breaks space view however, so let's compensate for that with an exponent that "fades" to 1 as we leave the atmosphere.
	float  rayHeight = AtmosphereHeight(rayStart);
	float  sampleDistributionExponent = 1 + clamp(1 - rayHeight / ATMOSPHERE_HEIGHT, 0, 1) * 8; // Slightly arbitrary max exponent of 9

	vec2 intersection = AtmosphereIntersection(rayStart, rayDir);
	rayLength = min(rayLength, intersection.y);
	if (intersection.x > 0)
	{
		// Advance ray to the atmosphere entry point
		rayStart += rayDir * intersection.x;
		rayLength -= intersection.x;
	}

	float  costh    = dot(rayDir, lightDir);
	float  phaseR   = PhaseRayleigh(costh);
	float  phaseM   = PhaseMie(costh);

	int    sampleCount  = 16;

	vec3 opticalDepth = vec3(0);
	vec3 rayleigh     = vec3(0);
	vec3 mie          = vec3(0);

	float  prevRayTime  = 0;

	for (int i = 0; i < sampleCount; i++) {
		float  rayTime = pow(float(i) / sampleCount, sampleDistributionExponent) * rayLength;
		// Because we are distributing the samples exponentially, we have to calculate the step size per sample.
		float  stepSize = (rayTime - prevRayTime);

		vec3 localPosition = rayStart + rayDir * rayTime;
		float  localHeight   = AtmosphereHeight(localPosition);
		vec3 localDensity  = AtmosphereDensity(localHeight);

		opticalDepth += localDensity * stepSize;

		// The atmospheric transmittance from rayStart to localPosition
		vec3 viewTransmittance = Absorb(opticalDepth);

		vec3 opticalDepthlight  = IntegrateOpticalDepth(localPosition, lightDir);
		// The atmospheric transmittance of light reaching localPosition
		vec3 lightTransmittance = Absorb(opticalDepthlight);

		rayleigh += viewTransmittance * lightTransmittance * phaseR * localDensity.x * stepSize;
		mie      += viewTransmittance * lightTransmittance * phaseM * localDensity.y * stepSize;

		prevRayTime = rayTime;
	}

	transmittance = Absorb(opticalDepth);

	return (rayleigh * C_RAYLEIGH + mie * C_MIE) * lightColor * EXPOSURE;
}


void main()
{    
    float x = uv.x * 2.0f - 1.0f;
    float y = (uv.y) * 2.0f - 1.0f; // uv.y * -1 for d3d
    float z = 1;
    vec4 position_s = vec4(x, y, z, 1.0f);
    vec4 position_v = invViewProj * position_s;
    vec3 div = position_v.xyz / position_v.w;

    vec3 rayDir = normalize(cameraPos.xyz - div);
    vec3 rayStart = cameraPos.xyz;
    float rayLength = INFINITY;


    vec3 transmittance;
    vec3 color = IntegrateScattering(rayStart, rayDir, rayLength, sunlightDir.xyz, sunlightColor.rgb, transmittance);

    out_color = vec4(color, 1.0);
}