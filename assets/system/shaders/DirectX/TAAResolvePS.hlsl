#include "include/bindless.hlsli"
#include "include/common.hlsli"
#include "include/packing.hlsli"

ROOT_CONSTANTS(TAAResolveConstants, rc)

// From GPU Gems: https://developer.download.nvidia.com/books/HTML/gpugems/gpugems_ch24.html
float FilterBlackmanHarris(float x);

// From TheRealMJP: https://github.com/TheRealMJP/MSAAFilter/blob/master/MSAAFilter/Resolve.hlsl
float FilterMitchellNetravali(float x);

// Also from TheRealMJP: http://vec3.ca/bicubic-filtering-in-fewer-taps/
float4 SampleTextureCatmullRom(in Texture2D<float4> tex, in SamplerState linearSampler, in float2 uv, in float2 texSize);

// From Play Dead Games (Inside): https://github.com/playdeadgames/temporal/blob/master/Assets/Shaders/TemporalReprojection.shader
float4 ClipAABB(float3 aabb_min, float3 aabb_max, float4 p, float4 q);


float4 main(in FULLSCREEN_TRIANGLE_VS_OUT inParams) : SV_Target0 {
    Texture2D color_texture = ResourceDescriptorHeap[rc.mColorTexture];
    Texture2D depth_texture = ResourceDescriptorHeap[rc.mDepthTexture];
    Texture2D history_texture = ResourceDescriptorHeap[rc.mHistoryTexture];
    Texture2D velocity_texture = ResourceDescriptorHeap[rc.mVelocityTexture];
    
    FrameConstants fc = gGetFrameConstants();
    
    float3 color_sample = color_texture[inParams.mPixelCoords.xy].rgb;
    
    if (fc.mFrameCounter == 0)
        return float4(color_sample, 1.0);
    
    // moments for variance clip
    float3 m1 = 0;
    float3 m2 = 0;
    float3 min_color = 10000.xxx;
    float3 max_color = -10000.xxx;
    
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            int2 pixel_pos = clamp(inParams.mPixelCoords.xy + int2(x, y), 0, rc.mRenderSize);
            float3 neighbor_sample = max(color_texture[pixel_pos].rgb, 0);
            
            min_color = min(min_color, neighbor_sample);
            max_color = max(max_color, neighbor_sample);
            
            m1 += neighbor_sample;
            m2 += neighbor_sample * neighbor_sample;
        }
    }
    
    float one_over_9 = rcp(9.0f);
    float3 avg_color = m1 * one_over_9;
    
    // Variance clipping from https://developer.download.nvidia.com/gameworks/events/GDC2016/msalvi_temporal_supersampling.pdf
    float3 mu = m1 * one_over_9;
    float3 sigma = sqrt(abs(m2 * one_over_9) - mu * mu);
    float gamma = 1.0f;
    float3 minc = mu - gamma * sigma;
    float3 maxc = mu + gamma * sigma;
    
    float2 velocity = velocity_texture[inParams.mPixelCoords.xy].xy;
    float2 history_uv = saturate(inParams.mScreenUV - velocity);
    
    float3 history_sample = SampleTextureCatmullRom(history_texture, SamplerLinearClamp, history_uv, rc.mRenderSize).rgb;
    //float3 history_sample = history_texture.Sample(SamplerLinearClamp, history_uv).rgb;
    
    // Salvi variance clip
    history_sample = ClipAABB(minc, maxc, float4(clamp(avg_color, min_color, max_color), 1.0), float4(history_sample, 1.0)).rgb;
    // variance clamp
    history_sample = clamp(history_sample, min_color, max_color);
    
    return float4(lerp(history_sample, color_sample, 0.1), 1.0);
}



float FilterMitchellNetravali(float x)
{
    float B = 1.0 / 3.0;
    float C = 1.0 / 3.0;
    float ax = abs(x);
    if (ax < 1)
    {
        return ((12 - 9 * B - 6 * C) * ax * ax * ax +
            (-18 + 12 * B + 6 * C) * ax * ax + (6 - 2 * B)) / 6;
    }
    else if ((ax >= 1) && (ax < 2))
    {
        return ((-B - 6 * C) * ax * ax * ax +
                (6 * B + 30 * C) * ax * ax + (-12 * B - 48 * C) *
                ax + (8 * B + 24 * C)) / 6;
    }
    else
    {
        return 0;
    }
}



float FilterBlackmanHarris(in float x)
{
    x = 1.0f - x;

    const float a0 = 0.35875f;
    const float a1 = 0.48829f;
    const float a2 = 0.14128f;
    const float a3 = 0.01168f;
    return saturate(a0 - a1 * cos(M_PI * x) + a2 * cos(2 * M_PI * x) - a3 * cos(3 * M_PI * x));
}



float4 SampleTextureCatmullRom(in Texture2D<float4> tex, in SamplerState linearSampler, in float2 uv, in float2 texSize)
{
    // We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
    // down the sample location to get the exact center of our "starting" texel. The starting texel will be at
    // location [1, 1] in the grid, where [0, 0] is the top left corner.
    float2 samplePos = uv * texSize;
    float2 texPos1 = floor(samplePos - 0.5f) + 0.5f;

    // Compute the fractional offset from our starting texel to our original sample location, which we'll
    // feed into the Catmull-Rom spline function to get our filter weights.
    float2 f = samplePos - texPos1;

    // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
    // These equations are pre-expanded based on our knowledge of where the texels will be located,
    // which lets us avoid having to evaluate a piece-wise function.
    float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    float2 w3 = f * f * (-0.5f + 0.5f * f);

    // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
    // simultaneously evaluate the middle 2 samples from the 4x4 grid.
    float2 w12 = w1 + w2;
    float2 offset12 = w2 / (w1 + w2);

    // Compute the final UV coordinates we'll use for sampling the texture
    float2 texPos0 = texPos1 - 1;
    float2 texPos3 = texPos1 + 2;
    float2 texPos12 = texPos1 + offset12;

    texPos0 /= texSize;
    texPos3 /= texSize;
    texPos12 /= texSize;

    float4 result = 0.0f;
    result += tex.SampleLevel(linearSampler, float2(texPos0.x, texPos0.y), 0.0f) * w0.x * w0.y;
    result += tex.SampleLevel(linearSampler, float2(texPos12.x, texPos0.y), 0.0f) * w12.x * w0.y;
    result += tex.SampleLevel(linearSampler, float2(texPos3.x, texPos0.y), 0.0f) * w3.x * w0.y;

    result += tex.SampleLevel(linearSampler, float2(texPos0.x, texPos12.y), 0.0f) * w0.x * w12.y;
    result += tex.SampleLevel(linearSampler, float2(texPos12.x, texPos12.y), 0.0f) * w12.x * w12.y;
    result += tex.SampleLevel(linearSampler, float2(texPos3.x, texPos12.y), 0.0f) * w3.x * w12.y;

    result += tex.SampleLevel(linearSampler, float2(texPos0.x, texPos3.y), 0.0f) * w0.x * w3.y;
    result += tex.SampleLevel(linearSampler, float2(texPos12.x, texPos3.y), 0.0f) * w12.x * w3.y;
    result += tex.SampleLevel(linearSampler, float2(texPos3.x, texPos3.y), 0.0f) * w3.x * w3.y;

    return result;
}


float4 ClipAABB(float3 aabb_min, float3 aabb_max, float4 p, float4 q)
{
    static const float FLT_EPS = 0.00000001f;
	// note: only clips towards aabb center (but fast!)
	float3 p_clip = 0.5 * (aabb_max + aabb_min);
    float3 e_clip = 0.5 * (aabb_max - aabb_min) + FLT_EPS;

	float4 v_clip = q - float4(p_clip, p.w);
	float3 v_unit = v_clip.xyz / e_clip;
	float3 a_unit = abs(v_unit);
	float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

	if (ma_unit > 1.0)
		return float4(p_clip, p.w) + v_clip / ma_unit;
	else
		return q;// point inside aabb
}