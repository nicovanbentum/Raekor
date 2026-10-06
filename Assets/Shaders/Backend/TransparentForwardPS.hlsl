#include "Include/Common.hlsli"
#include "Include/Bindless.hlsli"
#include "Include/Material.hlsli"
#include "Include/DDGI.hlsli"
#include "Include/Sky.hlsli"
#include "Include/RayTracing.hlsli"

struct VS_OUTPUT
{
    float4 sv_position      : SV_Position;
    float4 ws_position      : POS0;
    float4 curr_position    : POS1;
    float4 prev_position    : POS2;
    float2 texcoord         : TEXCOORD;
    float3 normal           : NORMAL;
    float3 tangent          : TANGENT;
    float3 bitangent        : BINORMAL;
};

struct PS_OUTPUT
{
    float4 color : SV_Target0;
    uint selection : SV_Target1;
};

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(TransparentForwardConstants, rc)

static TextureCube<float3> skycube_texture = ResourceDescriptorHeap[rc.mSkyCubeTexture];
static TextureCube<float3> diffuse_cube_texture = ResourceDescriptorHeap[rc.mDiffuseSkyCubeTexture];
static StructuredBuffer<RTLight> lights = ResourceDescriptorHeap[fc.mLightsBuffer];
static Texture2D<float2> brdf_lut_texture = ResourceDescriptorHeap[rc.mBrdfLutTexture];


float3 SampleEnvironmentSpecular(float3 inDirection, float inRoughness)
{
    uint width, height, levels;
    skycube_texture.GetDimensions(0, width, height, levels);

    return skycube_texture.SampleLevel(SamplerLinearClamp, inDirection, inRoughness * (levels - 1)) * fc.mSunColor.a;
}


PS_OUTPUT main(in VS_OUTPUT inParams, bool inIsFrontFace : SV_IsFrontFace)
{
    StructuredBuffer<RTGeometry> geometries = ResourceDescriptorHeap[fc.mInstancesBuffer];
    StructuredBuffer<RTMaterial> materials = ResourceDescriptorHeap[fc.mMaterialsBuffer];

    RTGeometry geometry = geometries[rc.mInstanceIndex];
    RTMaterial material = materials[geometry.mMaterialIndex];

    Texture2D albedo_texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.mAlbedoTexture)];
    Texture2D normals_texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.mNormalsTexture)];
    Texture2D emissive_texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.mEmissiveTexture)];
    Texture2D metallic_texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.mMetallicTexture)];
    Texture2D roughness_texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.mRoughnessTexture)];

    float4 sampled_albedo = albedo_texture.Sample(SamplerAnisoWrap, inParams.texcoord);
    float3 sampled_normal = normals_texture.Sample(SamplerAnisoWrap, inParams.texcoord).rgb;
    float3 sampled_emissive = emissive_texture.Sample(SamplerAnisoWrap, inParams.texcoord).rgb;
    float sampled_metallic = metallic_texture.Sample(SamplerAnisoWrap, inParams.texcoord).r;
    float sampled_roughness = roughness_texture.Sample(SamplerAnisoWrap, inParams.texcoord).r;

    sampled_normal = ReconstructNormalBC5(sampled_normal.xy);

    const float face_sign = inIsFrontFace ? 1.0 : -1.0;

    float3x3 TBN = float3x3(inParams.tangent, inParams.bitangent, inParams.normal * face_sign);
    float3 normal = normalize(mul(sampled_normal.xyz, TBN));

    Surface surface;
    surface.mNormal = normal;
    surface.mAlbedo = material.mAlbedo * sampled_albedo;
    surface.mMetallic = material.mMetallic * sampled_metallic;
    surface.mRoughness = material.mRoughness * sampled_roughness;
    surface.mEmissive = material.mEmissive.rgb * sampled_emissive;
    surface.mEnergyCompensation = 1.0.xxx;

    const float opacity = saturate(surface.mAlbedo.a);
    const float3 ws_pos = inParams.ws_position.xyz;
    const float3 Wo = normalize(fc.mCameraPosition.xyz - ws_pos);

    const float NdotV = saturate(dot(surface.mNormal, Wo));
    const float2 dfg = brdf_lut_texture.SampleLevel(SamplerLinearClamp, float2(NdotV, surface.mRoughness), 0.0).rg;
    const float3 F0 = surface.GetF0();
    const float3 specular_albedo = F0 * dfg.x + dfg.y;

    surface.mEnergyCompensation = GetEnergyCompensation(F0, dfg);

    const float3 diffuse_albedo = surface.mAlbedo.rgb * opacity;
    surface.mAlbedo.rgb = lerp(diffuse_albedo, surface.mAlbedo.rgb, surface.mMetallic);

    float3 total_radiance = surface.mEmissive * opacity;

    {
        float3 Wi = normalize(-fc.mSunDirection.xyz);
        float sun_shadow = 1.0;

        if (rc.mUseRayTracedShadows && dot(surface.mNormal, Wi) > 0.0)
        {
            RaytracingAccelerationStructure TLAS = ResourceDescriptorHeap[fc.mShadowTLAS];
            sun_shadow = TraceShadowRay(TLAS, ws_pos + inParams.normal * face_sign * 0.01, Wi, 0.0, 10000.0) ? 0.0 : 1.0;
        }

        total_radiance += EvaluateDirectionalLight(surface, fc.mSunColor, Wi, Wo) * sun_shadow;
    }

    for (int light_idx = 0; light_idx < fc.mNrOfLights; light_idx++)
    {
        RTLight light = lights[light_idx];
        float dist_to_light = length(light.mPosition.xyz - ws_pos);

        switch (light.mType)
        {
            case RT_LIGHT_TYPE_POINT:
            {
                float3 Wi = SamplePointLight(light, ws_pos);
                total_radiance += EvaluatePointLight(surface, light, Wi, Wo, dist_to_light);
            } break;

            case RT_LIGHT_TYPE_SPOT:
            {
                float3 Wi = SampleSpotLight(light, ws_pos);
                total_radiance += EvaluateSpotLight(surface, light, Wi, Wo, dist_to_light);
            } break;
        }
    }

    float3 indirect_specular = SampleEnvironmentSpecular(reflect(-Wo, surface.mNormal), surface.mRoughness);
    total_radiance += indirect_specular * specular_albedo * surface.mEnergyCompensation;

    float3 indirect_diffuse = rc.mUseIndirectDiffuse ?
        DDGISampleIrradiance(ws_pos, surface.mNormal, Wo, rc.mDDGIData) :
        diffuse_cube_texture.SampleLevel(SamplerLinearClamp, surface.mNormal, 0) * fc.mSunColor.a;

    total_radiance += indirect_diffuse * diffuse_albedo * (1.0 - surface.mMetallic) * (1.0 - specular_albedo);

    PS_OUTPUT output;
    output.color = float4(max(total_radiance, 0.0.xxx), opacity);
    output.selection = rc.mEntity;

    return output;
}
