 #ifndef COMMON_HLSLI
 #define COMMON_HLSLI

#include "Bindless.hlsli"
#include "Packing.hlsli"

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


float2 CalculateViewToScreenUV(float3 inPos, float4x4 inViewToClipMatrix)
{
    float4 clip_pos = mul(inViewToClipMatrix, float4(inPos, 1.0));
    clip_pos.xy /= clip_pos.w;
    clip_pos.xy = clip_pos.xy * 0.5 + 0.5;
    return clip_pos.xy;
}

float3x3 Adjugate(float4x4 m)
{
    return float3x3(cross(m[1].xyz, m[2].xyz),
                    cross(m[2].xyz, m[0].xyz),
                    cross(m[0].xyz, m[1].xyz));
}


void TransformToWorldSpace(inout RTVertex inVertex, float4x4 inLocalToWorldMatrix)
{
    inVertex.mPos = mul(inLocalToWorldMatrix, float4(inVertex.mPos, 1.0)).xyz;
    inVertex.mNormal = mul(Adjugate(inLocalToWorldMatrix), inVertex.mNormal);
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


// Building an Orthonormal Basis, Revisited
// http://jcgt.org/published/0006/01/01/
float3x3 BuildOrthonormalBasis(float3 n)
{
    float3 b1;
    float3 b2;

    if (n.z < 0.0)
    {
        const float a = 1.0 / (1.0 - n.z);
        const float b = n.x * n.y * a;
        b1 = float3(1.0 - n.x * n.x * a, -b, n.x);
        b2 = float3(b, n.y * n.y * a - 1.0, -n.y);
    }
    else
    {
        const float a = 1.0 / (1.0 + n.z);
        const float b = -n.x * n.y * a;
        b1 = float3(1.0 - n.x * n.x * a, b, -n.x);
        b2 = float3(b, 1.0 - n.y * n.y * a, -n.y);
    }

    return float3x3(
        b1.x, b2.x, n.x,
        b1.y, b2.y, n.y,
        b1.z, b2.z, n.z
    );
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
