#include "Include/Bindless.hlsli"
#include "Include/DDGI.hlsli"

struct VS_OUTPUT 
{
    uint index : INDEX;
    float4 position : SV_Position;
    float4 ws_position : POSITION;
    float3 normal   : NORMAL;
    float3 color    : COLOR;
};

struct VS_INPUT 
{
    float3 pos      : POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
};

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(DDGIData, rc)

VS_OUTPUT main (in VS_INPUT input, uint instance_id : SV_InstanceID) 
{
    VS_OUTPUT output;
    output.index = instance_id;
    
    float3 probe_ws_pos = DDGIGetRelocatedProbeWorldPos(instance_id, rc);
    
    float min_scale = DDGIGetMinProbeSpacing(DDGIGetVolume(DDGIGetProbeCascade(instance_id, rc), rc));
    float3 scaled_pos = input.pos * min_scale * rc.mProbeRadius;
    
    output.ws_position = float4(scaled_pos + probe_ws_pos, 1.0);
    output.position = mul(fc.mViewProjectionMatrix, output.ws_position);
    output.normal = normalize(input.normal);
    
    output.color = max(dot(input.normal, -fc.mSunDirection.xyz), 0).xxx;
    
    return output;
}
