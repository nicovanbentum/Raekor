#include "Include/Sky.hlsli"
#include "Include/Common.hlsli"
#include "Include/Bindless.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(SkyCubeRootConstants, rc)

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
    RWTexture2DArray<float3> sky_cube_texture = ResourceDescriptorHeap[rc.mSkyCubeTexture];
    
    uint width, height, layers;
    sky_cube_texture.GetDimensions(width, height, layers);
    
    float2 uv = (float2(gid.xy) + 0.5) / float2(width, height);
    float3 dir = normalize(GetCubemapDirection(uv, gid.z));
    
    float3 transmittance;
    sky_cube_texture[gid] = IntegrateScattering(0.xxx, normalize(dir), INFINITY, rc.mSunLightDirection, rc.mSunLightColor.rgb, transmittance);
}