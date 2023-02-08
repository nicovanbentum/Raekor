#include "include/bindless.hlsli"
#include "include/random.hlsli"

struct VS_OUTPUT {
    float4 mPos : SV_Position;
    float3 mPrevPos : PrevPosition;
    float3 mNormal : NORMAL;
    float height : Field0;
};

ROOT_CONSTANTS(GrassRenderRootConstants, grc)

static const float2 vertex_array[] =
{
    float2(-0.5, 0.0),
    float2(0.5,  0.0),
    float2(-0.5, 0.0),
    float2(0.5,  0.0),
    float2(-0.5, 0.0),
    float2(0.5,  0.0),
    float2(-0.5, 0.0),
    float2(0.5,  0.0),
    float2(-0.5, 0.0),
    float2(0.5,  0.0),
    float2(-0.5, 0.0),
    float2(0.5,  0.0),
    float2(-0.5, 0.0),
    float2(0.5,  0.0),
    float2(0.0,  0.0)
};

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


VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    FrameConstants fc = gGetFrameConstants();
    
    uint hash_state = input.instance_id;
    hash_state = pcg_hash(hash_state);
    
    float2 random = pcg_float2(hash_state);
    
    float blade_height = 2;
    float blade_width = 0.10;
    
    float height01 = float(input.vertex_id / 2) / 7.0;
    
    uint row = input.instance_id / 256;
    uint column = input.instance_id - (row * 256);
    
    float3 pos = float3(row, 0.0, column);
    float2 facing = pcg_float2(hash_state);
    //facing = float2(0, 1.0);
    
    pos.xz *= 0.5;
    pos.xz += random;
    
    float3 control_point0 = pos;
    float3 control_point2 = float3(pos.x, blade_height, pos.z);
    // tilt the tip of the blade in the direction its facing
    control_point2.xz += facing * grc.mTilt;
    float3 control_point1 = lerp(control_point0, control_point2, 0.5);
    
    
    //// bend the mid point 
    control_point1.xz += -facing * grc.mBend;
    control_point1.y = lerp(control_point1.y, control_point2.y, grc.mBend);
    
    float sin_time = sin(fc.mTime * 2.0) * 0.5 + 0.5;
    control_point2.xz += -grc.mWindDirection * sin_time;
    
    output.mPos.xyz = QuadraticBezierCurve(control_point0, control_point1, control_point2, height01);
    output.mPos.w = 1.0;
    
    float2 perp = normalize(cross(float3(facing.x, 0.0, facing.y), float3(0, 1, 0))).xz;
    
    if (input.vertex_id != 14)
    {
        if (input.vertex_id % 2)
            output.mPos.xz += perp * (blade_width / 2);
        else 
            output.mPos.xz += -perp * (blade_width / 2);
    }
    
    output.mPos = mul(fc.mViewProjectionMatrix, output.mPos);
    output.mNormal = float3(facing.x, 1.0, facing.y);
    output.height = height01;

    return output;
}