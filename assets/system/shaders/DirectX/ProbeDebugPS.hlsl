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

ROOT_CONSTANTS(ProbeDebugRootConstants, rc)


PS_OUTPUT main(in VS_OUTPUT input) {
    Texture2D<float2> probes_depth_texture = ResourceDescriptorHeap[rc.mProbesDepthTexture];
    Texture2DArray<float3> probes_irradiance_texture = ResourceDescriptorHeap[rc.mProbesIrradianceTexture];
    
    uint3 irradiance_texture_size = 0.xxx;
    probes_irradiance_texture.GetDimensions(irradiance_texture_size.x, irradiance_texture_size.y, irradiance_texture_size.z);
    
    uint3 probe_coord = GetDDGIProbeCoordinate(input.index, rc.mProbeCount.xyz);
    
    uint2 texture_uv = (probe_coord.xz * DDGI_PROBE_IRRADIANCE_RESOLUTION) / irradiance_texture_size.xy;
    float2 prob_uv = texture_uv + (OctEncode(input.normal) * (DDGI_PROBE_IRRADIANCE_RESOLUTION * (1.0 / irradiance_texture_size.xy)));
    
    uint layer = probe_coord.y;
    float3 color = probes_irradiance_texture.SampleLevel(SamplerLinearClamp, float3(prob_uv, layer), 0);
    
    PS_OUTPUT output;
    output.rendertarget0 = float4(color, 1.0);
    
    return output;
}