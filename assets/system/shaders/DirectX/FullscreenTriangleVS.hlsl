#include "include/common.hlsli"

FULLSCREEN_TRIANGLE_VS_OUT main(uint vertex_id : SV_VertexID) {
    FULLSCREEN_TRIANGLE_VS_OUT output;

    float x = float(((uint(vertex_id) + 2u) / 3u) % 2u);
    float y = float(((uint(vertex_id) + 1u) / 3u) % 2u);

    output.mPixelCoords = float4(x * 2.0 - 1.0, y * 2.0 - 1.0, 1.0, 1.0);
    output.mScreenUV = float2(x, 1.0 - y);

    return output;
}

