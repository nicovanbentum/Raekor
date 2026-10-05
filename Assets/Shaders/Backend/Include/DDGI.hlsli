#ifndef DDGI_HLSLI
#define DDGI_HLSLI

#include "Shared.hlsli"
#include "Random.hlsli"
#include "Common.hlsli"
#include "Bindless.hlsli"


uint Index2DTo1D(uint2 inCoord, uint inWidth) 
{
    return inCoord.x + inCoord.y * inWidth;
}


uint2 Index1DTo2D(uint inIndex, uint inWidth) 
{
    return uint2(inIndex % inWidth, inIndex / inWidth);
}

uint Index3Dto1D(uint3 inIndex, uint3 inCount) 
{
    return inIndex.x +
           inIndex.y * inCount.x +
           inIndex.z * inCount.x * inCount.y;
}

uint3 Index1DTo3D(uint inIndex, uint3 inCount) 
{
    return uint3(
        inIndex % inCount.x,
        (inIndex / inCount.x) % inCount.y,
        inIndex / (inCount.x * inCount.y)
    );
}

/*
    Octahedral and spherical fibonacci projections, code from Ray Tracing Gems 2 Chapter 3
*/
float SignNotZero(float inValue) 
{
    return (inValue >= 0.0) ? 1.0 : -1.0;
}

float2 SignNotZero(float2 inVec) 
{
    return float2(SignNotZero(inVec.x), SignNotZero(inVec.y));
}

float2 OctEncode(float3 inVec) 
{
    float l1norm = abs(inVec.x) + abs(inVec.y) + abs(inVec.z);
    float2 result = inVec.xy * (1.0 / l1norm);
    
    if (inVec.z < 0.0)
        result = (1.0 - abs(result.yx)) * SignNotZero(result.xy);
    
    return result;
}

float3 OctDecode(float2 inOct) 
{
    float3 v = float3(inOct.x, inOct.y, 1.0 - abs(inOct.x) - abs(inOct.y));
    if (v.z < 0.0)
        v.xy = (1.0 - abs(v.yx)) * SignNotZero(v.xy);
    
    return normalize(v);
}

float4 DDGIGetProbeDebugColor(uint inProbeIndex, uint3 inProbeCount) 
{
    return float4(Index1DTo3D(inProbeIndex, inProbeCount) & 1, 1.0);
}


uint DDGIGetProbesPerCascade(DDGIData inData)
{
    return inData.mProbeCount.x * inData.mProbeCount.y * inData.mProbeCount.z;
}


DDGIVolume DDGIGetVolume(uint inCascade, DDGIData inData)
{
    StructuredBuffer<DDGIVolume> volumes = ResourceDescriptorHeap[inData.mVolumesBuffer];
    return volumes[inCascade];
}


int3 DDGIWrapGridCoord(int3 inCoord, int3 inProbeCount)
{
    return ((inCoord % inProbeCount) + inProbeCount) % inProbeCount;
}


uint DDGIGetProbeIndex(int3 inGridCoord, DDGIVolume inVolume, DDGIData inData)
{
    int3 storage_coord = DDGIWrapGridCoord(inGridCoord + inVolume.mScrollOffset, inData.mProbeCount);
    return inVolume.mProbeOffset + Index3Dto1D(storage_coord, inData.mProbeCount);
}


uint DDGIGetProbeCascade(uint inProbeIndex, DDGIData inData)
{
    return inProbeIndex / DDGIGetProbesPerCascade(inData);
}


int3 DDGIGetProbeGridCoord(uint inProbeIndex, DDGIVolume inVolume, DDGIData inData)
{
    int3 storage_coord = Index1DTo3D(inProbeIndex - inVolume.mProbeOffset, inData.mProbeCount);
    return DDGIWrapGridCoord(storage_coord - inVolume.mScrollOffset, inData.mProbeCount);
}


int3 DDGIGetProbeCell(int3 inGridCoord, DDGIVolume inVolume)
{
    return inVolume.mOriginCell + inGridCoord;
}


float3 DDGIGetProbeWorldPos(int3 inGridCoord, DDGIVolume inVolume) 
{
    return inVolume.mCornerPosition + inVolume.mProbeSpacing * inGridCoord;
}


float3 DDGIGetRelocatedProbeWorldPos(uint inProbeIndex, DDGIData inData)
{
    StructuredBuffer<ProbeData> probe_buffer = ResourceDescriptorHeap[inData.mProbesDataBuffer];
    
    DDGIVolume volume = DDGIGetVolume(DDGIGetProbeCascade(inProbeIndex, inData), inData);
    int3 grid_coord = DDGIGetProbeGridCoord(inProbeIndex, volume, inData);
    
    ProbeData probe_data = probe_buffer[inProbeIndex];
    float3 offset = all(probe_data.cell == DDGIGetProbeCell(grid_coord, volume)) ? probe_data.offset : 0.xxx;
    
    return DDGIGetProbeWorldPos(grid_coord, volume) + offset;
}


float3 DDGIGetProbeRayDirection(uint inRayIndex, float4x4 inRandomRotationMatrix)
{
    return normalize(mul((float3x3)inRandomRotationMatrix, SphericalFibonnaci(inRayIndex, DDGI_RAYS_PER_PROBE)));
}


template<typename T>
T DDGISampleProbe(uint inProbeIndex, float3 inDir, uint inProbeTexels, uint inTotalTexels, Texture2D<T> inTexture) 
{
    uint2 texture_size = 0.xx;
    inTexture.GetDimensions(texture_size.x, texture_size.y);
    float2 pixel_uv_size = 1.0 / texture_size;
    
    // Map 1D probe index (SV_InstanceID) to 2D array index, multiply that by the nr of texels per probe, add half the texel area to arrive at exactly the center
    uint2 probe_pixel_center = Index1DTo2D(inProbeIndex, DDGI_PROBES_PER_ROW) * inTotalTexels + (inTotalTexels * 0.5);
    
    // Integer pixel coordinates to UV space
    float2 probe_uv_center = float2(probe_pixel_center) / texture_size;
    
    // Calculate the size of the inner texels (probe texels without border) in UV space
    float2 inner_texels_uv_size = (inProbeTexels) * pixel_uv_size;
    
    // Octahedral mapping from normal to UV, the uv is in range [-1, 1] around the center, it depicts where we are on the inner texels
    float2 oct_uv = clamp(OctEncode(inDir), -1.0, 1.0);
    
    // Calculate the final uv.
    float2 sample_uv = probe_uv_center + (oct_uv * (inner_texels_uv_size * 0.5));
    
    return T(inTexture.SampleLevel(SamplerLinearClamp, sample_uv, 0));
}


float3 DDGISampleIrradianceProbe(uint inProbeIndex, float3 inDir, Texture2D<float4> inTexture) 
{
    return DDGISampleProbe(inProbeIndex, inDir, DDGI_IRRADIANCE_TEXELS_NO_BORDER, DDGI_IRRADIANCE_TEXELS, inTexture).rgb;
}


float2 DDGISampleDepthProbe(uint inProbeIndex, float3 inDir, Texture2D<float2> inTexture) 
{
    return DDGISampleProbe(inProbeIndex, inDir, DDGI_DEPTH_TEXELS_NO_BORDER, DDGI_DEPTH_TEXELS, inTexture);
}


float DDGIGetMinProbeSpacing(DDGIVolume inVolume)
{
    return min(min(inVolume.mProbeSpacing.x, inVolume.mProbeSpacing.y), inVolume.mProbeSpacing.z);
}


float3 DDGIGetSurfaceBias(float3 inNormal, float3 inViewDir, DDGIVolume inVolume)
{
    return (inNormal * 0.2f + inViewDir * 0.8f) * (0.75f * DDGIGetMinProbeSpacing(inVolume)) * 0.3f;
}


float3 DDGISampleVolumeIrradiance(float3 inWsPos, float3 inNormal, float3 inViewDir, DDGIVolume inVolume, DDGIData inData) 
{
    float3 biased_ws_pos = inWsPos + DDGIGetSurfaceBias(inNormal, inViewDir, inVolume);
    
    int3 base_probe_coord = clamp(int3(floor((biased_ws_pos - inVolume.mCornerPosition) / inVolume.mProbeSpacing)), 0, inData.mProbeCount - 1);
    float3 base_probe_ws_pos = DDGIGetProbeWorldPos(base_probe_coord, inVolume);
    float3 alpha = saturate((biased_ws_pos - base_probe_ws_pos) / inVolume.mProbeSpacing);
    
    Texture2D<float2> depth_texture = ResourceDescriptorHeap[inData.mProbesDepthTexture];
    Texture2D<float4> irradiance_texture = ResourceDescriptorHeap[inData.mProbesIrradianceTexture];
    StructuredBuffer<ProbeData> probe_buffer = ResourceDescriptorHeap[inData.mProbesDataBuffer];
    
    float4 irradiance = 0.0.xxxx;
    
    for (int i = 0; i < 8; i++) 
    {
        int3 cube_indices = int3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
        int3 probe_coord = clamp(base_probe_coord + cube_indices, 0, inData.mProbeCount - 1);
        
        uint probe_index = DDGIGetProbeIndex(probe_coord, inVolume, inData);
        ProbeData probe_data = probe_buffer[probe_index];
        
        if (probe_data.inactive || any(probe_data.cell != DDGIGetProbeCell(probe_coord, inVolume)))
            continue;
        
        float3 probe_ws_pos = DDGIGetProbeWorldPos(probe_coord, inVolume) + probe_data.offset;
        float3 pos_to_probe_dir = normalize(probe_ws_pos - inWsPos);
        
        float weight = square((dot(pos_to_probe_dir, inNormal) + 1.0f) * 0.5f) + 0.2f;
        
        if (inData.mUseChebyshev)
        {
            float3 probe_to_biased_pos = biased_ws_pos - probe_ws_pos;
            float probe_to_biased_dist = length(probe_to_biased_pos);
            
            float2 moments = DDGISampleDepthProbe(probe_index, probe_to_biased_pos / max(probe_to_biased_dist, 1e-5f), depth_texture);
            float variance = abs(square(moments.x) - moments.y);
            
            float chebyshev_weight = 1.0f;
            
            if (probe_to_biased_dist > moments.x)
            {
                float v = probe_to_biased_dist - moments.x;
                chebyshev_weight = variance / (variance + square(v));
                chebyshev_weight = max(chebyshev_weight * chebyshev_weight * chebyshev_weight, 0.0f);
            }
            
            weight *= max(0.05f, chebyshev_weight);
        }
        
        weight = max(0.000001f, weight);
        
        const float crush_threshold = 0.2f;
        if (weight < crush_threshold)
            weight *= weight * weight * (1.0f / square(crush_threshold));
        
        float3 trilinear = lerp(1.0f - alpha, alpha, float3(cube_indices));
        weight *= trilinear.x * trilinear.y * trilinear.z;
        
        float3 sampled_irradiance = DDGISampleIrradianceProbe(probe_index, inNormal, irradiance_texture);
        
        irradiance += float4(sampled_irradiance * weight, weight);
    }
    
    if (irradiance.w > 0.0001f)
        irradiance.rgb /= irradiance.w;

    return irradiance.rgb;
}


float3 DDGISampleIrradiance(float3 inWsPos, float3 inNormal, float3 inViewDir, DDGIData inData) 
{
    for (uint cascade = 0; cascade < inData.mCascadeCount; cascade++)
    {
        DDGIVolume volume = DDGIGetVolume(cascade, inData);
        
        float3 grid_pos = (inWsPos - volume.mCornerPosition) / volume.mProbeSpacing;
        float3 edge_distances = min(grid_pos, float3(inData.mProbeCount - 1) - grid_pos);
        float edge_distance = min(min(edge_distances.x, edge_distances.y), edge_distances.z);
        
        bool has_next_cascade = cascade + 1 < inData.mCascadeCount;
        
        if (edge_distance < 0.0f && has_next_cascade)
            continue;
        
        float3 irradiance = DDGISampleVolumeIrradiance(inWsPos, inNormal, inViewDir, volume, inData);
        float cascade_blend = saturate(edge_distance * 0.5f);
        
        if (cascade_blend < 1.0f && has_next_cascade)
        {
            float3 next_irradiance = DDGISampleVolumeIrradiance(inWsPos, inNormal, inViewDir, DDGIGetVolume(cascade + 1, inData), inData);
            irradiance = lerp(next_irradiance, irradiance, cascade_blend);
        }
        
        return irradiance;
    }
    
    return 0.0.xxx;
}

#endif // DDGI_HLSLI