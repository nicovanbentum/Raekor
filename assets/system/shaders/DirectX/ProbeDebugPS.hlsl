#include "include/bindless.hlsli"
#include "include/packing.hlsli"
#include "include/ddgi.hlsli"

struct VS_OUTPUT {
    uint index : INDEX;
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float3 color : COLOR;
};

struct PS_OUTPUT {
    float4 rendertarget0: SV_Target0;
};

ROOT_CONSTANTS(DDGIData, rc)


PS_OUTPUT main(in VS_OUTPUT input) {
    Texture2D<float2> probes_depth_texture = ResourceDescriptorHeap[rc.mProbesDepthTexture];
    Texture2D<float4> probes_irradiance_texture = ResourceDescriptorHeap[rc.mProbesIrradianceTexture];
    
    float3 irradiance = DDGISampleIrradianceProbe(input.index, input.normal, probes_irradiance_texture);
    
    PS_OUTPUT output;
    //output.rendertarget0 = DDGIGetProbeDebugColor(input.index, rc.mProbeCount);
    output.rendertarget0 = float4(irradiance, 1.0);
    
    return output;
}