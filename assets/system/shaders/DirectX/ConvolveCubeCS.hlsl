#include "include/sky.hlsli"
#include "include/common.hlsli"
#include "include/random.hlsli"
#include "include/bindless.hlsli"

ROOT_CONSTANTS(ConvolveCubeRootConstants, rc)

#define PI 3.14159265359
#define NR_OF_SAMPLES 16

float3 GetCubemapDirection(float2 inUV, uint inFace)
{
    float2 clip = inUV * 2.0 - 1.0;

    switch (inFace)
    {
        case 0: return float3(1.0, -clip.yx);
        case 1: return float3(-1.0, -clip.y, clip.x);
        case 2: return float3(clip.x, 1.0, clip.y);
        case 3: return float3(clip.x, -1.0, -clip.y);
        case 4: return float3(clip.x, -clip.y, 1.0);
        case 5: return float3(-clip, -1.0);
    }

    return 0.xxx;
}

[numthreads(8, 8, 1)]
void main(uint3 gid : SV_DispatchThreadID)
{
    TextureCube<float3> cube_texture = ResourceDescriptorHeap[rc.mCubeTexture];
    RWTexture2DArray<float3> convolved_texture = ResourceDescriptorHeap[rc.mConvolvedCubeTexture];
    
    uint width, height, layers;
    convolved_texture.GetDimensions(width, height, layers);
    
    float3 dir = GetCubemapDirection(float2(gid.xy) / float2(width, height), gid.z);
    
    float3 irradiance = 0.xxx;
    
    for (int i = 0; i < NR_OF_SAMPLES; i++)
    {
        float2 rand = Hammersley2D(i, NR_OF_SAMPLES);
        float3 sample_dir = normalize(dir + SampleCosineWeightedHemisphere(float2(rand.x, rand.y)));
        irradiance += cube_texture.Sample(SamplerLinearClamp, sample_dir).rgb;
    }
    
    irradiance = PI * irradiance * (1.0 / float(NR_OF_SAMPLES));
    
    convolved_texture[gid] = irradiance;
}