#include "Include/Shared.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"
#include "Include/Bindless.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(SSAOTraceRootConstants, rc)

static const Texture2D<float> g_DepthTexture = ResourceDescriptorHeap[rc.mDepthTexture];
static const Texture2D<float4> g_GBufferTexture = ResourceDescriptorHeap[rc.mGBufferTexture];
static const RWTexture2D<float4> g_OutputTexture = ResourceDescriptorHeap[rc.mOutputTexture];

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    if (any(threadID.xy >= rc.mDispatchSize.xy))
        return;
    
    float2 pixel_center = float2(threadID.xy) + float2(0.5, 0.5);
    float2 screen_uv = pixel_center / rc.mDispatchSize;
    
    float depth = g_DepthTexture[threadID.xy];
    
    if (depth == 1.0)
    {
        g_OutputTexture[threadID.xy] = 1.0;
        return;
    }
    
    float3 ws_position = ReconstructWorldPosition(screen_uv, depth, fc.mInvViewProjectionMatrix);
    float3 vs_position = mul(fc.mViewMatrix, float4(ws_position, 1.0)).xyz;
    
    float3 ws_normal = UnpackNormal(asuint(g_GBufferTexture[threadID.xy]));
    float3 vs_normal = normalize(mul((float3x3)fc.mViewMatrix, ws_normal));
    
    float3 world_up = abs(vs_normal.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
    float3 tangent = normalize(cross(world_up, vs_normal));
    float3 bitangent = normalize(cross(vs_normal, tangent));
    float3x3 TBN = float3x3(tangent, bitangent, vs_normal);

    uint rng = TeaHash(((threadID.y << 16) | threadID.x), fc.mFrameCounter + 1);

    float occlusion = 0.0;
    
    for (int sample_index = 0; sample_index < rc.mSamples; sample_index++)
    {
        //float4 blue_noise = SampleBlueNoise(pixel_center, fc.mFrameCounter);
        //float ign = InterleavedGradientNoise(pixel_center);
        //float2 rand = Hammersley2D(sample_index, rc.mSamples);
        float2 rand = pcg_float2(rng);
        //rand = blue_noise.xy;

        float3 sample = SampleCosineWeightedHemisphere(rand.xy);
        
        //float3 sample_vs_pos = vs_position + vs_normal;
        float range = rc.mRadius * lerp(0.2, 1.0, pcg_float(rng));
        float3 sample_vs_pos = vs_position + normalize(mul(TBN, sample)) * range;
        float4 sample_cs_pos = mul(fc.mProjectionMatrix, float4(sample_vs_pos, 1.0));
        
        float2 sample_uv = sample_cs_pos.xy / sample_cs_pos.w;
        sample_uv.x = sample_uv.x * 0.5 + 0.5;
        sample_uv.y = -sample_uv.y * 0.5 + 0.5;
        
        float sample_depth = g_DepthTexture.SampleLevel(SamplerPointClamp, sample_uv, 0);
        float3 sample_ws_pos = ReconstructWorldPosition(sample_uv, sample_depth, fc.mInvViewProjectionMatrix);
        sample_depth = mul(fc.mViewMatrix, float4(sample_ws_pos, 1.0)).z;
        
        float rangecheck = smoothstep(0.0, 1.0, 0.5 / abs(vs_position.z - sample_depth));
        occlusion += (sample_depth >= sample_vs_pos.z + rc.mBias ? 1.0 : 0.0) * rangecheck;
    }
    
    if (occlusion > 0.0f)
        occlusion /= rc.mSamples;
    
    occlusion = 1.0 - occlusion;
    
    g_OutputTexture[threadID.xy] = float4(occlusion.xxx, 1.0);
}