#include "include/bindless.hlsli"

#define GOLDEN_ANGLE 2.39996323
#define MAX_BLUR_SIZE 20.0
#define RAD_SCALE 0.5 // Smaller = nicer blur, larger = faster

ROOT_CONSTANTS(DepthOfFieldRootConstants, rc)

float GetBlurSize(float depth, float focusPoint, float focusScale)
{
	float coc = clamp((1.0 / focusPoint - 1.0 / depth) * focusScale, -1.0, 1.0);
	return abs(coc) * MAX_BLUR_SIZE;
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID) 
{
    Texture2D depth_texture = ResourceDescriptorHeap[rc.mDepthTexture];
    Texture2D input_texture = ResourceDescriptorHeap[rc.mInputTexture];
    RWTexture2D<float4> output_texture = ResourceDescriptorHeap[rc.mOutputTexture];

    uint width, height;
    output_texture.GetDimensions(width, height);
    float2 pixel_size = float2(1.0, 1.0) / uint2(width, height);

    const float2 pixel_center = float2(threadID.xy) + float2(0.5, 0.5);
    float2 screen_uv = pixel_center / uint2(width, height);

    float center_depth = depth_texture.Sample(SamplerPointClamp, screen_uv).r * rc.mFarPlane;

	float center_size = GetBlurSize(center_depth, rc.mFocusPoint, rc.mFocusScale);
    float3 color = input_texture.Sample(SamplerPointClamp, screen_uv).rgb;
	
	float tot = 1.0;
	float radius = RAD_SCALE;
	
    for (float angle = 0.0; radius < MAX_BLUR_SIZE; angle += GOLDEN_ANGLE)
	{
        float2 tc = screen_uv + float2(cos(angle), sin(angle)) * pixel_size * radius;
        float3 sample_color = input_texture.Sample(SamplerPointClamp, tc).rgb;
        float sample_depth = depth_texture.Sample(SamplerPointClamp, tc).r * rc.mFarPlane;
        float sample_size = GetBlurSize(sample_depth, rc.mFocusPoint, rc.mFocusScale);
		
        if (sample_depth > center_depth)
            sample_size = clamp(sample_size, 0.0, center_size * 2.0);
		
        float m = smoothstep(radius - 0.5, radius + 0.5, sample_size);

        color += lerp(color / tot, sample_color, m);

		tot += 1.0;
        radius += RAD_SCALE / radius;
	}

    output_texture[threadID.xy] = float4(color / tot, 1.0);
}