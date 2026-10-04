#include "Include/Bindless.hlsli"
#include "Include/Packing.hlsli"
#include "Include/DDGI.hlsli"
#include "Include/Common.hlsli"

struct VS_OUTPUT {
    uint index : INDEX;
    float4 position : SV_Position;
    float4 ws_position : POSITION;
    float3 normal : NORMAL;
    float3 color : COLOR;
};

struct PS_OUTPUT {
    float4 rendertarget0: SV_Target0;
};

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(DDGIData, rc)

float min3(float3 inValue)
{
    return min(min(inValue.x, inValue.y), inValue.z);
}

PS_OUTPUT main(in VS_OUTPUT input) {
    Texture2D<float2> probes_depth_texture = ResourceDescriptorHeap[rc.mProbesDepthTexture];
    Texture2D<float4> probes_irradiance_texture = ResourceDescriptorHeap[rc.mProbesIrradianceTexture];
    
    float3 irradiance = DDGISampleIrradianceProbe(input.index, input.normal, probes_irradiance_texture);
    float2 depth = normalize(DDGISampleDepthProbe(input.index, input.normal, probes_depth_texture));
    
    DDGIData data = rc;
    uint3 probe_coord = Index1DTo3D(input.index, rc.mProbeCount);
    float3 probe_ws_pos = DDGIGetProbeWorldPos(probe_coord, data);
    
    float d = length(probe_ws_pos - input.ws_position.xyz);
    
    PS_OUTPUT output;
    //output.rendertarget0 = DDGIGetProbeDebugColor(input.index, rc.mProbeCount);
    output.rendertarget0 = float4(irradiance, 1.0);
    
    return output;
}