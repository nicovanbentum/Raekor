 #ifndef COMMON_HLSLI
 #define COMMON_HLSLI

#include "bindless.hlsli"
#include "packing.hlsli"

#define M_PI 3.14159265358979323846

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


void TransformToWorldSpace(inout RTVertex inVertex, float4x4 inLocalToWorldMatrix, float4x4 inInvLocalToWorldMatrix)
{
    inVertex.mPos = mul(inLocalToWorldMatrix, float4(inVertex.mPos, 1.0)).xyz;
    inVertex.mNormal = mul((float3x3) transpose(inInvLocalToWorldMatrix), inVertex.mNormal);
    inVertex.mTangent = mul(inLocalToWorldMatrix, float4(inVertex.mTangent, 0.0)).xyz;
}


float MapRange(float value, float start1, float stop1, float start2, float stop2) {
    return start2 + (stop2 - start2) * ((value - start1) / (stop1 - start1));
}


template<typename T> 
T square(T inValue) { return inValue * inValue; }


float LuminanceLinear(float3 inLinearRGB)
{
    return dot(inLinearRGB, float3(0.2127, 0.7152, 0.0722));
}

void Discard(bool inDiscard)
{
    if (inDiscard) discard;
}

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
