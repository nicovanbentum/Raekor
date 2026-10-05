#include "Include/Sky.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"
#include "Include/Bindless.hlsli"

ROOT_CONSTANTS(ConvolveCubeRootConstants, rc)

#define PI 3.14159265359
#define NR_OF_SAMPLES 512

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
    
    float2 uv = (float2(gid.xy) + 0.5) / float2(width, height);
    float3 dir = normalize(GetCubemapDirection(uv, gid.z));
    float3x3 basis = BuildOrthonormalBasis(dir);
    
    uint cube_width, cube_height, cube_levels;
    cube_texture.GetDimensions(0, cube_width, cube_height, cube_levels);
    
    const float texel_solid_angle = 4.0 * PI / (6.0 * cube_width * cube_width);
    
    float3 irradiance = 0.xxx;
    
    for (int i = 0; i < NR_OF_SAMPLES; i++)
    {
        float2 rand = Hammersley2D(i, NR_OF_SAMPLES);
        float3 local_dir = SampleCosineWeightedHemisphere(rand);
        float3 sample_dir = normalize(mul(basis, local_dir));
        
        float pdf = max(local_dir.z, 1e-4) / PI;
        float sample_solid_angle = 1.0 / (NR_OF_SAMPLES * pdf);
        float lod = clamp(0.5 * log2(sample_solid_angle / texel_solid_angle) + 1.0, 0.0, cube_levels - 1.0);
        
        irradiance += cube_texture.SampleLevel(SamplerLinearWrap, sample_dir, lod);
    }
    
    irradiance /= NR_OF_SAMPLES;
    
    convolved_texture[gid] = irradiance;
}