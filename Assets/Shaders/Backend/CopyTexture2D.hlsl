#include "Include/Bindless.hlsli"
#include "Include/Packing.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"
#include "Include/DDGI.hlsli"

ROOT_CONSTANTS(CopyTextureConstants, rc)

float4 main(in FULLSCREEN_TRIANGLE_VS_OUT inParams) : SV_Target0
{
    Texture2D<float4> src_texture = ResourceDescriptorHeap[rc.mSrcTexture];
    return src_texture.Sample(SamplerPointClamp, inParams.mScreenUV);
}