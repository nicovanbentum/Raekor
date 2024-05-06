#include "include/common.hlsli"

FULLSCREEN_TRIANGLE_VS_OUT main(uint vertex_id : SV_VertexID) {
    FULLSCREEN_TRIANGLE_VS_OUT output;

    output.mScreenUV = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.mPixelCoords = float4(output.mScreenUV * float2(2, -2) + float2(-1, 1), 0, 1);

    return output;
}

