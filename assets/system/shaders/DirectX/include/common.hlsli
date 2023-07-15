 #ifndef COMMON_HLSLI
 #define COMMON_HLSLI

#include "bindless.hlsli"
#include "packing.hlsli"

#define M_PI 3.14159265358979323846

struct FULLSCREEN_TRIANGLE_VS_OUT {
    float4 mPixelCoords : SV_Position;
    float2 mScreenUV    : TEXCOORD;
};

// TODO: nuke this lmao
float4x4 inverse(float4x4 m) {
    float n11 = m[0][0], n12 = m[1][0], n13 = m[2][0], n14 = m[3][0];
    float n21 = m[0][1], n22 = m[1][1], n23 = m[2][1], n24 = m[3][1];
    float n31 = m[0][2], n32 = m[1][2], n33 = m[2][2], n34 = m[3][2];
    float n41 = m[0][3], n42 = m[1][3], n43 = m[2][3], n44 = m[3][3];

    float t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 * n44;
    float t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 * n44;
    float t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 * n44;
    float t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34;

    float det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;
    float idet = 1.0f / det;

    float4x4 ret;

    ret[0][0] = t11 * idet;
    ret[0][1] = (n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 * n44) * idet;
    ret[0][2] = (n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 * n44) * idet;
    ret[0][3] = (n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 * n43) * idet;

    ret[1][0] = t12 * idet;
    ret[1][1] = (n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 * n44) * idet;
    ret[1][2] = (n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 * n44) * idet;
    ret[1][3] = (n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 * n43) * idet;

    ret[2][0] = t13 * idet;
    ret[2][1] = (n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 * n44) * idet;
    ret[2][2] = (n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 * n44) * idet;
    ret[2][3] = (n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 * n43) * idet;

    ret[3][0] = t14 * idet;
    ret[3][1] = (n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 * n34) * idet;
    ret[3][2] = (n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 * n34) * idet;
    ret[3][3] = (n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 * n33) * idet;

    return ret;
}


float3 ReconstructWorldPosition(float2 inUV, float inDepth, float4x4 inClipToWorldMatrix) {
    float x = inUV.x * 2.0f - 1.0f;
    float y = (1.0 - inUV.y) * 2.0f - 1.0f;
    float4 position_s = float4(x, y, inDepth, 1.0f);
    float4 position_v = mul(inClipToWorldMatrix, position_s);
    return position_v.xyz / position_v.w;
}


void TransformToWorldSpace(inout RTVertex inVertex, float4x4 inLocalToWorldMatrix)
{
    inVertex.mPos = mul(inLocalToWorldMatrix, float4(inVertex.mPos, 1.0)).xyz;
    inVertex.mNormal = mul((float3x3) transpose(inverse(inLocalToWorldMatrix)), inVertex.mNormal);
    inVertex.mTangent = mul(inLocalToWorldMatrix, float4(inVertex.mTangent, 0.0)).xyz;
}


float MapRange(float value, float start1, float stop1, float start2, float stop2) {
    return start2 + (stop2 - start2) * ((value - start1) / (stop1 - start1));
}


template<typename T> 
T square(T inValue) { return inValue * inValue; }


void ResetDebugLineCount() {
    FrameConstants fc = gGetFrameConstants();
    RWByteAddressBuffer args_buffer = ResourceDescriptorHeap[fc.mDebugLinesIndirectArgsBuffer];

    uint original_value;
    args_buffer.InterlockedExchange(0, 0, original_value); // sets VertexCountPerInstance to 0
    args_buffer.InterlockedExchange(4, 1, original_value); // sets InstanceCount to 1
}


void AddDebugLine(float3 inP1, float3 inP2, float4 inColor1, float4 inColor2) {
    FrameConstants fc = gGetFrameConstants();
    
    RWByteAddressBuffer args_buffer = ResourceDescriptorHeap[fc.mDebugLinesIndirectArgsBuffer];
    RWStructuredBuffer<float4> vertex_buffer = ResourceDescriptorHeap[fc.mDebugLinesVertexBuffer];
    
    uint vertex_offset;
    args_buffer.InterlockedAdd(0, 2, vertex_offset);
    
    vertex_buffer[vertex_offset] = float4(inP1, asfloat(Float4ToRGBA8(inColor1)));
    vertex_buffer[vertex_offset + 1] = float4(inP2, asfloat(Float4ToRGBA8(inColor2)));
}

#endif // COMMON_HLSLI
