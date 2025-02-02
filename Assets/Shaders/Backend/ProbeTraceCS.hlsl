#include "Include/Bindless.hlsli"
#include "Include/Packing.hlsli"
#include "Include/Sky.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"
#include "Include/DDGI.hlsli"
#include "Include/Material.hlsli"
#include "Include/RayTracing.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(ProbeTraceRootConstants, rc)

void ResetDebugLineCount()
{
    RWByteAddressBuffer args_buffer = ResourceDescriptorHeap[fc.mDebugLinesIndirectArgsBuffer];

    uint original_value;
    args_buffer.InterlockedExchange(0, 0, original_value); // sets VertexCountPerInstance to 0
    args_buffer.InterlockedExchange(4, 1, original_value); // sets InstanceCount to 1
}


void AddDebugLine(float3 inP1, float3 inP2, float4 inColor1, float4 inColor2)
{
    RWByteAddressBuffer args_buffer = ResourceDescriptorHeap[fc.mDebugLinesIndirectArgsBuffer];
    RWStructuredBuffer<float4> vertex_buffer = ResourceDescriptorHeap[fc.mDebugLinesVertexBuffer];
    
    uint vertex_offset;
    args_buffer.InterlockedAdd(0, 2, vertex_offset);
    
    vertex_buffer[vertex_offset] = float4(inP1, asfloat(Float4ToRGBA8(inColor1)));
    vertex_buffer[vertex_offset + 1] = float4(inP2, asfloat(Float4ToRGBA8(inColor2)));
}

[numthreads(DDGI_TRACE_SIZE, 1, 1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    RaytracingAccelerationStructure TLAS = ResourceDescriptorHeap[fc.mTLAS];
    StructuredBuffer<RTGeometry> geometries = ResourceDescriptorHeap[fc.mInstancesBuffer];
    StructuredBuffer<RTMaterial> materials  = ResourceDescriptorHeap[fc.mMaterialsBuffer];
    
    RWTexture2D<float> depth_texture         = ResourceDescriptorHeap[rc.mDDGIData.mRaysDepthTexture];
    RWTexture2D<float3> irradiance_texture   = ResourceDescriptorHeap[rc.mDDGIData.mRaysIrradianceTexture];

    TextureCube<float3> skycube_texture = ResourceDescriptorHeap[rc.mSkyCubeTexture];

    uint ray_index = threadID.x;
    uint probe_index = threadID.y;
    uint2 ray_texture_index = uint2(ray_index, probe_index);
    
    float3 ray_dir = SphericalFibonnaci(ray_index, DDGI_RAYS_PER_PROBE);
    ray_dir = normalize(mul((float3x3)rc.mRandomRotationMatrix, ray_dir));
    
    float3 probe_ws_pos = DDGIGetProbeWorldPos(Index1DTo3D(probe_index, rc.mDDGIData.mProbeCount), rc.mDDGIData);
    
    RayDesc ray;
    ray.TMin = 0.01;
    ray.TMax = 10000.0;
    ray.Origin = probe_ws_pos;
    ray.Direction = ray_dir;
    
    RayQuery<RAY_FLAG_FORCE_OPAQUE> query;
    query.TraceRayInline(TLAS, RAY_FLAG_FORCE_OPAQUE, 0xFF, ray);
    while (query.Proceed()) {}

    float hitT = length(rc.mDDGIData.mProbeSpacing);
    float3 irradiance = 0.xxx;

    if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        hitT = ray.TMin + query.CommittedRayT();

        if (!query.CommittedTriangleFrontFace())
        {
            depth_texture[ray_texture_index] = -hitT;
            irradiance_texture[ray_texture_index] = float3(0, 0, 0);
            return;
        }
        
        RTGeometry geometry = geometries[query.CommittedInstanceID()];
        RTVertex vertex = CalculateVertexFromGeometry(geometry, query.CommittedPrimitiveIndex(), query.CommittedTriangleBarycentrics());
        TransformToWorldSpace(vertex, geometry.mWorldTransform);
        RTMaterial material = materials[geometry.mMaterialIndex];
        
        Surface surface;
        surface.FromHit(vertex, material);
        surface.mNormal = vertex.mNormal; // use the vertex normal, texture based detail is lost anyway
        
        irradiance = surface.mEmissive;
        const float3 Wo = -ray.Direction;

        {
            uint rng = 0;
            float3 Wi = SampleDirectionalLight(fc.mSunDirection.xyz, fc.mSunConeAngle, pcg_float2(rng));
        
            bool hit = TraceShadowRay(TLAS, vertex.mPos + vertex.mNormal * 0.01, Wi, 0.1f, 1000.0f);
            
            if (!hit)
                irradiance += EvaluateDirectionalLight(surface, fc.mSunColor, Wi, Wo);
        }
        
        // TODO: make probe textures persistent, at this point in the rendergraph they don't exist yet
        // Infinite bounces!
        if (fc.mFrameCounter > 0)
        {
            irradiance += surface.mAlbedo.rgb * DDGISampleIrradiance(vertex.mPos, vertex.mNormal, rc.mDDGIData);
        }
    }
    else
    {
        irradiance = skycube_texture.SampleLevel(SamplerLinearClamp, ray.Direction, 0);
        irradiance = max(irradiance, 0.0.xxx) * fc.mSunColor.a;
        
        hitT = ray.TMax;
    }
    
    if (probe_index == rc.mDebugProbeIndex) {
       // Resets indirect draw args VertexCount to 0 and InstanceCount to 1
       if (ray_index == 0)
           ResetDebugLineCount();
        
       float4 debug_ray_color = float4(irradiance, 1.0);
       float3 debug_ray_start = ray.Origin + ray.Direction * 0.25;
       float3 debug_ray_end   = ray.Origin + ray.Direction * hitT;
        
       // InterlockedAdd( 2 ) to the VertexCount, use the original value as write index into the line vertex buffer
       //AddDebugLine(debug_ray_start, debug_ray_end, debug_ray_color, debug_ray_color);
    }
    
    
    depth_texture[ray_texture_index] = hitT;
    irradiance_texture[ray_texture_index] = irradiance;
    // irradiance_texture[ray_texture_index] = DDGIGetProbeDebugColor(probe_index, rc.mDDGIData.mProbeCount).rgb;
}