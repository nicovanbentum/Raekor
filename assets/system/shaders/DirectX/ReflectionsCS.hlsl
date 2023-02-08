#include "include/bindless.hlsli"
#include "include/packing.hlsli"
#include "include/common.hlsli"
#include "include/random.hlsli"

Vertex InterpolateVertices(Vertex v0, Vertex v1, Vertex v2, float3 inBaryCentrics)
{
    Vertex vertex;
    vertex.mPos = v0.mPos * inBaryCentrics.x + v1.mPos * inBaryCentrics.y + v2.mPos * inBaryCentrics.z;
    vertex.mTexCoord = v0.mTexCoord * inBaryCentrics.x + v1.mTexCoord * inBaryCentrics.y + v2.mTexCoord * inBaryCentrics.z;
    vertex.mNormal = v0.mNormal * inBaryCentrics.x + v1.mNormal * inBaryCentrics.y + v2.mNormal * inBaryCentrics.z;
    vertex.mTangent = v0.mTangent * inBaryCentrics.x + v1.mTangent * inBaryCentrics.y + v2.mTangent * inBaryCentrics.z;
    
    return vertex;
}

ROOT_CONSTANTS(ReflectionsRootConstants, rc)

float3 ReconstructPosFromDepth(float2 uv, float depth, float4x4 InvVP) {
    float x = uv.x * 2.0f - 1.0f;
    float y = (1.0 - uv.y) * 2.0f - 1.0f;
    float4 position_s = float4(x, y, depth, 1.0f);
    float4 position_v = mul(InvVP, position_s);
    return position_v.xyz / position_v.w;
}


[numthreads(8,8,1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    Texture2D<float4> gbuffer_texture = ResourceDescriptorHeap[rc.mGbufferRenderTexture];
    Texture2D<float> gbuffer_depth_texture = ResourceDescriptorHeap[rc.mGbufferDepthTexture];
    RWTexture2D<float4> result_texture = ResourceDescriptorHeap[rc.mShadowMaskTexture];
    RaytracingAccelerationStructure TLAS = ResourceDescriptorHeap[rc.mTLAS];
    StructuredBuffer<Instance> instances = ResourceDescriptorHeap[rc.mInstancesBuffer];
    StructuredBuffer<Material> materials = ResourceDescriptorHeap[rc.mMaterialsBuffer];
    
    FrameConstants fc = gGetFrameConstants();

    const float2 pixel_center = float2(threadID.xy) + float2(0.5, 0.5);
    float2 screen_uv = pixel_center / rc.mDispatchSize;


    float depth = gbuffer_depth_texture[threadID.xy];
    if (depth >= 1.0) {
        result_texture[threadID.xy] = float4(0.0, 1.0, 0.0, 1.0);
        return;
    }
    
    const float3 normal = UnpackNormal(asuint(gbuffer_texture[threadID.xy]).y);
    const float3 position = ReconstructPosFromDepth(screen_uv, depth, fc.mInvViewProjectionMatrix);
    
    const float3 Wo = normalize(fc.mCameraPosition.xyz - position.xyz);
    const float3 Wi = normalize(reflect(Wo, normal));
    
    RayDesc ray;
    ray.TMin = 0.1;
    ray.TMax = 1000.0;
    ray.Origin = position + normal * 0.01; // TODO: find a more robust method? offsetRay from ray tracing gems 
                                            // expects a geometric normal, which most deferred renderers dont write out
    ray.Direction = normalize(reflect(Wo, normal));

    uint ray_flags =  RAY_FLAG_FORCE_OPAQUE |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
    
    RayQuery< RAY_FLAG_FORCE_OPAQUE |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;

    query.TraceRayInline(TLAS, ray_flags, 0xFF, ray);
    query.Proceed();

    if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        Instance instance = instances[query.CommittedInstanceIndex()];
        
        StructuredBuffer<uint3>  index_buffer  = ResourceDescriptorHeap[NonUniformResourceIndex(instance.mIndexBuffer)];
        StructuredBuffer<Vertex> vertex_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(instance.mVertexBuffer)];
        
        const uint3 indices = index_buffer[query.CommittedPrimitiveIndex()];
        const Vertex v0 = vertex_buffer[indices.x];
        const Vertex v1 = vertex_buffer[indices.y];
        const Vertex v2 = vertex_buffer[indices.z];
        
        float2 hit_bary = query.CommittedTriangleBarycentrics();
        const float3 barycentrics = float3(1.0 - hit_bary.x - hit_bary.y, hit_bary.x, hit_bary.y);
        const Vertex vertex = InterpolateVertices(v0, v1, v2, barycentrics);
        
        Material material = materials[instance.mMaterialIndex.x];
        Texture2D albedo_texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.mAlbedoTexture)];
        
        float4 albedo = material.mAlbedo * albedo_texture.Sample(SamplerAnisoClamp, vertex.mTexCoord);
        
        result_texture[threadID.xy] = albedo;
    }
    else
    {
        result_texture[threadID.xy] = float4(0.xxx, 1.0);
    }
}