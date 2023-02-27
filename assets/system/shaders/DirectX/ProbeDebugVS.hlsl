#include "include/bindless.hlsli"
#include "include/ddgi.hlsli"

struct VS_OUTPUT {
    uint index : INDEX;
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float3 color    : COLOR;
};

struct VS_INPUT {
    float3 pos      : POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
};

ROOT_CONSTANTS(ProbeDebugRootConstants, rc)

VS_OUTPUT main (in VS_INPUT input, uint instance_id : SV_InstanceID) {
    VS_OUTPUT output;
    output.index = instance_id;
    
    FrameConstants fc = gGetFrameConstants();
    
    float3 probe_ws_pos = GetDDGIProbeWorldPos(instance_id, rc.mProbeCount.xyz, rc.mBBmin.xyz, rc.mBBmax.xyz);
    
    output.position = mul(fc.mViewProjectionMatrix, float4(input.pos + probe_ws_pos, 1.0));
    output.normal = normalize(input.normal);
    
    // output.color = SampleIrradianceProbes(rc.IrradianceProbesParams, position, normal);
    output.color = max(dot(input.normal, -fc.mSunDirection.xyz), 0).xxx;
    
    return output;
}
