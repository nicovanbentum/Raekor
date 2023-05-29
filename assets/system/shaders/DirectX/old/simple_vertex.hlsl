struct Vertex {
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

struct VS_OUTPUT {
    float4 pos : SV_POSITION;
    float3 wpos : WORLDPOS;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BINORMAL;
    float2 uv : TEXCOORD;
};

cbuffer Constants : register(b0) {
    float4   mLightPosition;
    float4   mCameraPosition;
    float4x4 mModelMatrix;
    float4x4 mViewMatrix;
    float4x4 mProjectionMatrix;
}

StructuredBuffer<Vertex> VertexBuffer : register(t0);

VS_OUTPUT main(in uint inVertexID : SV_VertexID)
{
    Vertex vertex = VertexBuffer[inVertexID];

    VS_OUTPUT output;
    output.wpos = mul(mModelMatrix, float4(vertex.pos, 1.0));
    output.pos = mul(mul(mProjectionMatrix, mViewMatrix), float4(output.wpos, 1.0));
    output.normal = normalize(vertex.normal);
    output.tangent = normalize(vertex.tangent);
    output.tangent = normalize(output.tangent - dot(output.tangent, output.normal) * output.normal);
    output.bitangent = normalize(cross(output.normal, output.tangent));
    output.uv = vertex.uv;
    return output;
}