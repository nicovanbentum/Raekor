#include "include/bindless.hlsli"
#include "include/common.hlsli"

ROOT_CONSTANTS(BloomRootConstants, rc)

float3 Upscale(Texture2D inSrcTexture, uint inMip, float2 inUV, float2 inSrcResolutionRcp);
float3 Downscale(Texture2D inSrcTexture, uint inMip, float2 inUV, float2 inSrcResolutionRcp);


[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    if (any(threadID.xy >= rc.mDispatchSize.xy))
        return;
    
    Texture2D src_texture = ResourceDescriptorHeap[rc.mSrcTexture];
    RWTexture2D<float4> dst_texture = ResourceDescriptorHeap[rc.mDstTexture];
    
    const float2 pixel_center = float2(threadID.xy) + float2(0.5, 0.5);
    float2 screen_uv = pixel_center / rc.mDispatchSize;
    
    float4 result = float4(1, 0, 0, 1);
    
#if defined(DO_UPSCALE)
    
    result.rgb = Upscale(src_texture, rc.mSrcMipLevel, screen_uv, rc.mSrcSizeRcp);

#elif defined(DO_DOWNSCALE)
    
    result.rgb = Downscale(src_texture, rc.mSrcMipLevel, screen_uv, rc.mSrcSizeRcp);

#endif
    
    dst_texture[threadID.xy] = result;
}


float3 Upscale(Texture2D inSrcTexture, uint inMip, float2 inUV, float2 inSrcResolutionRcp)
{
    // The filter kernel is applied with a radius, specified in texture
    // coordinates, so that the radius will vary across mip resolutions.
    float x = inSrcResolutionRcp.x;
    float y = inSrcResolutionRcp.y;

    // Take 9 samples around current texel:
    // a - b - c
    // d - e - f
    // g - h - i
    // === ('e' is the current texel) ===
    float3 a = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x - x, inUV.y + y), inMip).rgb;
    float3 b = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x, inUV.y + y), inMip).rgb;
    float3 c = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x + x, inUV.y + y), inMip).rgb;

    float3 d = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x - x, inUV.y), inMip).rgb;
    float3 e = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x, inUV.y), inMip).rgb;
    float3 f = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x + x, inUV.y), inMip).rgb;

    float3 g = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x - x, inUV.y - y), inMip).rgb;
    float3 h = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x, inUV.y - y), inMip).rgb;
    float3 i = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x + x, inUV.y - y), inMip).rgb;

    // Apply weighted distribution, by using a 3x3 tent filter:
    //  1   | 1 2 1 |
    // -- * | 2 4 2 |
    // 16   | 1 2 1 |
    float3 upsample = e * 4.0;
    upsample += (b + d + f + h) * 2.0;
    upsample += (a + c + g + i);
    upsample *= 1.0 / 16.0;
    
    return upsample;
}


float3 Downscale(Texture2D inSrcTexture, uint inMip, float2 inUV, float2 inSrcResolutionRcp)
{
    float x = inSrcResolutionRcp.x;
    float y = inSrcResolutionRcp.y;
    
    float3 a = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x - 2 * x, inUV.y + 2 * y), inMip).rgb;
    float3 b = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x, inUV.y + 2 * y), inMip).rgb;
    float3 c = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x + 2 * x, inUV.y + 2 * y), inMip).rgb;

    float3 d = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x - 2 * x, inUV.y), inMip).rgb;
    float3 e = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x, inUV.y), inMip).rgb;
    float3 f = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x + 2 * x, inUV.y), inMip).rgb;

    float3 g = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x - 2 * x, inUV.y - 2 * y), inMip).rgb;
    float3 h = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x, inUV.y - 2 * y), inMip).rgb;
    float3 i = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x + 2 * x, inUV.y - 2 * y), inMip).rgb;

    float3 j = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x - x, inUV.y + y), inMip).rgb;
    float3 k = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x + x, inUV.y + y), inMip).rgb;
    float3 l = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x - x, inUV.y - y), inMip).rgb;
    float3 m = inSrcTexture.SampleLevel(SamplerLinearClamp, float2(inUV.x + x, inUV.y - y), inMip).rgb;

    float3 downsample = e * 0.125;
    downsample += (a + c + g + i) * 0.03125;
    downsample += (b + d + f + h) * 0.0625;
    downsample += (j + k + l + m) * 0.125;
    
    downsample = max(downsample, 0.0001);
    
    return downsample;
}