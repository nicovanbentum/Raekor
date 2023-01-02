#include "include/bindless.hlsli"

struct VS_OUTPUT {
    float4 position : SV_Position;
    float height : Field0;
};

float3 EvalulateCubicBezierCurve(float3 p0, float3 p1, float3 p2, float3 p3, float t) {
    float3 a = lerp(p0, p1, t);
    float3 b = lerp(p1, p2, t);
    float3 c = lerp(p2, p3, t);
    float3 d = lerp(a, b, t);
    float3 e = lerp(b, c, t);
    return lerp(d, e, t);
}



VS_OUTPUT main(uint vertex_id : SV_VertexID) {
    VS_OUTPUT output;

    FrameConstants fc = gGetFrameConstants();
    
    float blade_height = 1;
    float height01 = float(vertex_id / 2) / 7.0;
    
    if (vertex_id == 14) {
        output.position = float4(0, blade_height * 1.3, 0, 1.0);
    }
    else {
        output.position = float4(0, 0, 0, 1.0);
        output.position.y = height01 * blade_height;
        if (vertex_id % 2)
            output.position.z = 0.05;
        else
            output.position.z = -0.05;
    }
    
    output.position = mul(fc.mViewProjectionMatrix, output.position);
    
    output.height = height01;

    return output;
}