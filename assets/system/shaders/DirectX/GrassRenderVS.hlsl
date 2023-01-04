#include "include/bindless.hlsli"

struct VS_OUTPUT {
    float4 position : SV_Position;
    float height : Field0;
};

ROOT_CONSTANTS(GrassRenderConstants, grc)

float3 QuadraticBezierCurve(float3 p0, float3 p1, float3 p2, float t) {
    return p0 * pow((1 - t), 2) + p1 * 2 * (1 - t) * t + p2 * pow(t, 2);
}

float3 QuadraticBezierCurveDerivative(float3 p0, float3 p1, float3 p2, float t) {
    return p0 * (2 * t - 2) + (2 * p2 - 4 * p1) * t + 2 * p1;
}

struct VS_INPUT {
    uint vertex_id : SV_VertexID;
    uint instance_id : SV_InstanceID;
};


// from https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
uint pcg_hash(inout uint in_state)
{
    uint state = in_state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    in_state = (word >> 22u) ^ word;
    return in_state;
}

float pcg_float(inout uint in_state)
{
    return pcg_hash(in_state) / float(uint(0xffffffff));
}

float2 pcg_vec2(inout uint in_state)
{
    return float2(pcg_float(in_state), pcg_float(in_state));
}


VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    FrameConstants fc = gGetFrameConstants();
    
    uint hash_state = input.instance_id;
    hash_state = pcg_hash(hash_state);
    
    float2 random = pcg_vec2(hash_state);
    
    float blade_height = 1;
    float blade_width = 0.10;
    
    float height01 = float(input.vertex_id / 2) / 7.0;
    
    uint row = input.instance_id / 256;
    uint column = input.instance_id - (row * 256);
    
    float3 pos = float3(row, 0.0, column);
    float2 facing = pcg_vec2(hash_state);
    
    pos.xz *= 0.5;
    pos.xz += random;
    
    float3 control_point0 = pos;
    float3 control_point2 = float3(pos.x, blade_height, pos.z);
    float3 control_point1 = lerp(control_point0, control_point2, 0.5);
    
    // tilt the tip of the blade in the direction its facing
    control_point2.xz += facing * grc.mTilt;
    
    //// bend the mid point 
    control_point1.xz += -facing * grc.mBend;
    control_point1.y = lerp(control_point1.y, control_point2.y, grc.mBend);
    
    output.position.xyz = QuadraticBezierCurve(control_point0, control_point1, control_point2, height01);
    output.position.w = 1.0;
    
    if (input.vertex_id != 14)
    {
        if (input.vertex_id % 2)
            output.position.xz += facing * (blade_width / 2);
        else 
            output.position.xz += -facing * (blade_width / 2);
    }
    
    output.position = mul(fc.mViewProjectionMatrix, output.position);
    
    output.height = height01;

    return output;
}