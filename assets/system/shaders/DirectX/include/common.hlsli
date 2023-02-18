 #ifndef COMMON_HLSLI
 #define COMMON_HLSLI

struct FULLSCREEN_TRIANGLE_VS_OUT {
    float4 mPixelCoords : SV_Position;
    float2 mScreenUV    : TEXCOORD;
};


float3 ReconstructWorldPosition(float2 inUV, float inDepth, float4x4 inClipToWorldMatrix) {
    float x = inUV.x * 2.0f - 1.0f;
    float y = (1.0 - inUV.y) * 2.0f - 1.0f;
    float4 position_s = float4(x, y, inDepth, 1.0f);
    float4 position_v = mul(inClipToWorldMatrix, position_s);
    return position_v.xyz / position_v.w;
}


float MapRange(float value, float start1, float stop1, float start2, float stop2) {
    return start2 + (stop2 - start2) * ((value - start1) / (stop1 - start1));
}

#endif // COMMON_HLSLI
