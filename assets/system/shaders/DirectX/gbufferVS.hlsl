#include "include/bindless.hlsli"

struct VS_OUTPUT {
    float4 sv_position      : SV_Position;
    float4 curr_position    : POS0;
    float4 prev_position    : POS1;
    float2 texcoord         : TEXCOORD;
    float3 normal           : NORMAL;
    float3 tangent          : TANGENT;
    float3 bitangent        : BINORMAL;
};

struct VS_INPUT {
    float3 pos      : POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
};

struct RootConstants {
    float4 albedo;
    uint4 textures;
    float4 properties;
};

ROOT_CONSTANTS(RootConstants, root_constants)

struct Transform {
    float4x4 mat;
};

VS_OUTPUT main(in VS_INPUT input) {
    VS_OUTPUT output;
    
    FrameConstants fc = gGetFrameConstants();
    
    output.normal = normalize(input.normal);
	output.tangent = normalize(input.tangent);
    output.tangent = normalize(output.tangent - dot(output.tangent, output.normal) * output.normal);
    output.bitangent = normalize(cross(output.normal, output.tangent));
    output.texcoord = input.texcoord;
    
    output.curr_position = mul(fc.mViewProjectionMatrix, float4(input.pos, 1.0));
    output.prev_position = mul(fc.mPrevViewProjectionMatrix, float4(input.pos, 1.0));
    output.sv_position = output.curr_position;
    return output;
}
