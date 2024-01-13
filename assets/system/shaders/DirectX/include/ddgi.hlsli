#ifndef DDGI_HLSLI
#define DDGI_HLSLI

#include "shared.h"
#include "common.hlsli"
#include "bindless.hlsli"

/*
TODO: 
    - Sample irradiance when calculating current irradiance to get some form of infinite bounces 
*/

uint Index2DTo1D(uint2 inCoord, uint inWidth) {
    return inCoord.x + inCoord.y * inWidth;
}


uint2 Index1DTo2D(uint inIndex, uint inWidth) {
    return uint2(inIndex % inWidth, inIndex / inWidth);
}

uint Index3Dto1D(uint3 inIndex, uint3 inCount) {
    return inIndex.x +
           inIndex.y * inCount.x +
           inIndex.z * inCount.x * inCount.y;
}

uint3 Index1DTo3D(uint inIndex, uint3 inCount) {
    return uint3(
        inIndex % inCount.x,
        (inIndex / inCount.x) % inCount.y,
        inIndex / (inCount.x * inCount.y)
    );
}

/*
    Octahedral and spherical fibonacci projections, code from Ray Tracing Gems 2 Chapter 3
*/
float SignNotZero(float inValue) {
    return (inValue >= 0.0) ? 1.0 : -1.0;
}

float2 SignNotZero(float2 inVec) {
    return float2(SignNotZero(inVec.x), SignNotZero(inVec.y));
}

float2 OctEncode(float3 inVec) {
    float l1norm = abs(inVec.x) + abs(inVec.y) + abs(inVec.z);
    float2 result = inVec.xy * (1.0 / l1norm);
    
    if (inVec.z < 0.0)
        result = (1.0 - abs(result.yx)) * SignNotZero(result.xy);
    
    return result;
}

float3 OctDecode(float2 inOct) {
    float3 v = float3(inOct.x, inOct.y, 1.0 - abs(inOct.x) - abs(inOct.y));
    if (v.z < 0.0)
        v.xy = (1.0 - abs(v.yx)) * SignNotZero(v.xy);
    
    return normalize(v);
}

static const float PHI = sqrt(5) * 0.5 + 0.5;

float3 SphericalFibonnaci(uint i, uint n) {
    float fraction = (i * (PHI - 1)) - floor(i * (PHI - 1));
    float phi = 2.0 * M_PI * fraction;
    float cos_theta = 1.0 - (2.0 * i + 1.0) * (1.0 / n);
    float sin_theta = sqrt(saturate(1.0 - cos_theta * cos_theta));
    return normalize(float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta));
}

float4 DDGIGetProbeDebugColor(uint inProbeIndex, uint3 inProbeCount) {
    return float4(Index1DTo3D(inProbeIndex, inProbeCount) & 1, 1.0);
}


float3 DDGIGetProbeWorldPos(uint3 inProbeCoord, DDGIData inData) {
    return inData.mCornerPosition + inData.mProbeSpacing * inProbeCoord;
}


template<typename T>
T DDGISampleProbe(uint inProbeIndex, float3 inDir, uint inProbeTexels, uint inTotalTexels, Texture2D<T> inTexture) {
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


float3 DDGISampleIrradianceProbe(uint inProbeIndex, float3 inDir, Texture2D<float4> inTexture) {
    return DDGISampleProbe(inProbeIndex, inDir, DDGI_IRRADIANCE_TEXELS_NO_BORDER, DDGI_IRRADIANCE_TEXELS, inTexture).rgb;
}


float2 DDGISampleDepthProbe(uint inProbeIndex, float3 inDir, Texture2D<float2> inTexture) {
    return DDGISampleProbe(inProbeIndex, inDir, DDGI_DEPTH_TEXELS_NO_BORDER, DDGI_DEPTH_TEXELS, inTexture);
}


float3 DDGISampleIrradiance(float3 inWsPos, float3 inNormal, DDGIData inData) {
    // Calculate normalized position within the 8 surrounding probes, used for trilinear interpolation
    uint3 start_probe_coord = floor((inWsPos - inData.mCornerPosition) / inData.mProbeSpacing);
    float3 start_probe_ws_pos = DDGIGetProbeWorldPos(start_probe_coord, inData);
    float3 ws_pos_01 = saturate((inWsPos - start_probe_ws_pos) / inData.mProbeSpacing);
    
    Texture2D<float2> depth_texture = ResourceDescriptorHeap[inData.mProbesDepthTexture];
    Texture2D<float4> irradiance_texture = ResourceDescriptorHeap[inData.mProbesIrradianceTexture];
    
    float4 irradiance = 0.0.xxxx;
    
    for (int i = 0; i < 8; i++) {
        // Calculate probe indices [0, 0, 0] to [1, 1, 1] , add that to the starting probe coord, retrieve the world position
        uint3 cube_indices = uint3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
        uint3 current_probe_coord = start_probe_coord + cube_indices;
        
        uint probe_index = Index3Dto1D(current_probe_coord, inData.mProbeCount);
        float3 probe_ws_pos = DDGIGetProbeWorldPos(current_probe_coord, inData);
                
        float3 pos_to_probe_dir = normalize(probe_ws_pos - inWsPos);
        
        // Initialize the weight to wrap shading
        float weight = saturate(dot(pos_to_probe_dir, inNormal));
        
        //// Chebyshev visibility test
        //float2 depth = DDGISampleDepthProbe(probe_index, -pos_to_probe_dir, depth_texture);
        //float r = length(probe_ws_pos - inWsPos);
        //float mean = depth.r, mean2 = depth.g;
        
        //if (r < mean)
        //{
        //    float variance = abs(square(mean) - mean2);
        //    weight *= variance / (variance + square(r - mean));
        //}

        // Calculate trilinear interpolation weight
        float3 tri = lerp(1.0 - ws_pos_01, ws_pos_01, cube_indices);
        float tri_weight = tri.x * tri.y * tri.z;
        weight *= max(tri_weight, 0.001);
        
        // Sample the probe's irradiance texels
        float3 sampled_irradiance = DDGISampleIrradianceProbe(probe_index, inNormal, irradiance_texture);
        
        uint3 debug_color = current_probe_coord & 1;
        
        // Accumulate weighted irradiance
        irradiance += float4(sampled_irradiance.rgb * weight, weight);
    }
    
    if (irradiance.w > 0.0)
        irradiance.rgb /= irradiance.w;

    return irradiance.rgb;
}

#endif // DDGI_HLSLI