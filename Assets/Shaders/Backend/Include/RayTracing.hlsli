#ifndef RT_HLSLI
#define RT_HLSLI

#include "Shared.hlsli"


RTVertex InterpolateVertices(RTVertex v0, RTVertex v1, RTVertex v2, float3 inBaryCentrics) {
    RTVertex vertex;
    vertex.mPos = v0.mPos * inBaryCentrics.x + v1.mPos * inBaryCentrics.y + v2.mPos * inBaryCentrics.z;
    vertex.mTexCoord = v0.mTexCoord * inBaryCentrics.x + v1.mTexCoord * inBaryCentrics.y + v2.mTexCoord * inBaryCentrics.z;
    vertex.mNormal = v0.mNormal * inBaryCentrics.x + v1.mNormal * inBaryCentrics.y + v2.mNormal * inBaryCentrics.z;
    vertex.mTangent = v0.mTangent * inBaryCentrics.x + v1.mTangent * inBaryCentrics.y + v2.mTangent * inBaryCentrics.z;
    
    return vertex;
}


RTVertex CalculateVertexFromGeometry(RTGeometry inGeometry, uint inPrimitiveIndex, float2 inBaryCentrics) {
    StructuredBuffer<uint3> index_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(inGeometry.mIndexBuffer)];
    StructuredBuffer<RTVertex> vertex_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(inGeometry.mVertexBuffer)];
        
    const uint3 indices = index_buffer[inPrimitiveIndex];
    const RTVertex v0 = vertex_buffer[indices.x];
    const RTVertex v1 = vertex_buffer[indices.y];
    const RTVertex v2 = vertex_buffer[indices.z];
        
    // float3 vert_normal = normalize(cross(v0.mPos - v1.mPos, v0.mPos - v2.mPos));
        
    const float3 barycentrics = float3(1.0 - inBaryCentrics.x - inBaryCentrics.y, inBaryCentrics.x, inBaryCentrics.y);
    return InterpolateVertices(v0, v1, v2, barycentrics);
}


bool TraceShadowRay(RaytracingAccelerationStructure inTLAS, float3 inRayPos, float3 inRayDir, float inTMin, float inTMax)
{
    RayDesc shadow_ray;
    shadow_ray.Origin = inRayPos;
    shadow_ray.Direction = inRayDir;
    shadow_ray.TMin = inTMin;
    shadow_ray.TMax = inTMax;
                
    uint shadow_ray_flags = RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
    RayQuery < RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER > query;

    query.TraceRayInline(inTLAS, shadow_ray_flags, 0xFF, shadow_ray);
    query.Proceed();
                
    return query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}

#endif