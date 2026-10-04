#include "Include/Common.hlsli"
#include "Include/Bindless.hlsli"
#include "Include/Material.hlsli"
#include "Include/DDGI.hlsli"
#include "Include/Sky.hlsli"

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
};

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(TransparentForwardConstants, rc)

static Texture2D<float2> shadow_texture = ResourceDescriptorHeap[rc.shadowMaskTexture];
static Texture2D<float4> reflections_texture = ResourceDescriptorHeap[rc.reflectionsTexture];
static Texture2D<float4> indirect_diffuse_texture = ResourceDescriptorHeap[rc.indirectDiffuseTexture];
static TextureCube<float3> skycube_texture = ResourceDescriptorHeap[rc.skyCubeTexture];
static TextureCube<float3> diffuse_cube_texture = ResourceDescriptorHeap[rc.diffuseSkyCubeTexture];
static StructuredBuffer<RTLight> lights = ResourceDescriptorHeap[fc.mLightsBuffer];
static Texture2D<float2> brdf_lut_texture = ResourceDescriptorHeap[rc.brdfLutTexture];

float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0.xxx - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float3 ComputeIBL(Surface surface, float3 Wo)
{
    float3 r = reflect(-Wo, surface.mNormal.rgb);
    float3 diffuse_color = (1.0f - surface.mMetallic) * surface.mAlbedo.rgb;
    float3 Ld = diffuse_cube_texture.Sample(SamplerLinearClamp, r) * diffuse_color;
  
    // TODO: generate a pre-filtered HDR cubemap for roughness LOD
    float lod = 5.0 * surface.mRoughness;
    float3 env_map_sample = skycube_texture.SampleLevel(SamplerLinearWrap, r, lod);
    
    float cos_theta = max(dot(surface.mNormal, Wo), 0.0);
    float3 F0 = lerp(0.04, surface.mAlbedo.rgb, surface.mMetallic);
    float3 F = fresnelSchlickRoughness(cos_theta, F0, surface.mRoughness);
    
    float a = surface.mRoughness * surface.mRoughness;
    float2 brdf = brdf_lut_texture.SampleLevel(SamplerLinearClamp, float2(cos_theta, a), 0.0).rg;
    
    float3 Lr = (F * brdf.x + brdf.y) * env_map_sample;
    return Ld + Lr;
}

PS_OUTPUT main(in VS_OUTPUT inParams) {
    PS_OUTPUT output;

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
    if (sampled_albedo.a < 0.9)
        discard;
    
    float3 sampled_normal = normals_texture.Sample(SamplerAnisoWrap, inParams.texcoord).rgb; // alpha channel unused
    float3 sampled_emissive = emissive_texture.Sample(SamplerAnisoWrap, inParams.texcoord).rgb; // alpha channel unused
    float sampled_metallic = metallic_texture.Sample(SamplerAnisoWrap, inParams.texcoord).r; // value swizzled across all channels, just get Red
    float sampled_roughness = roughness_texture.Sample(SamplerAnisoWrap, inParams.texcoord).r; // value swizzled across all channels, just get Red
        
    //sampled_normal = sampled_normal * 2.0 - 1.0;
    sampled_normal = ReconstructNormalBC5(sampled_normal.xy);

    float3x3 TBN = float3x3(inParams.tangent, inParams.bitangent, inParams.normal);
    float3 normal = normalize(mul(sampled_normal.xyz, TBN));
    //normal = normalize(input.normal);

    Surface surface;
    surface.mNormal = normal;
    surface.mAlbedo = material.mAlbedo * sampled_albedo;
    surface.mMetallic = material.mMetallic * sampled_metallic;
    surface.mRoughness = material.mRoughness * sampled_roughness;
    surface.mEmissive = material.mEmissive.rgb * sampled_emissive;
    
    float2 curr_pos = (inParams.curr_position.xyz / inParams.curr_position.w).xy - fc.mJitter;
    float2 prev_pos = (inParams.prev_position.xyz / inParams.prev_position.w).xy - fc.mPrevJitter;
    
    float2 motionvectors = (curr_pos - prev_pos);
    motionvectors.xy *= float2(0.5, -0.5);
    
    const float3 Wo = normalize(fc.mCameraPosition.xyz - inParams.ws_position.xyz);
    
    // output.selection = rc.mEntity;
    
    float3 total_radiance = surface.mEmissive;
    
        // evaluate DirectionalLight 
    {
        float3 Wi = normalize(-fc.mSunDirection.xyz);
        float sun_shadow = shadow_texture.SampleLevel(SamplerLinearClamp, inParams.texcoord, 0).r;
        total_radiance += EvaluateDirectionalLight(surface, fc.mSunColor, Wi, Wo) * sun_shadow;
    }
    
    uint2 group_index = uint2(inParams.texcoord.xy) / LIGHT_CULL_TILE_SIZE;
    RWByteAddressBuffer light_count_buffer = ResourceDescriptorHeap[rc.lights.mLightGridBuffer];
    RWByteAddressBuffer light_index_buffer = ResourceDescriptorHeap[rc.lights.mLightIndicesBuffer];
        
    uint light_count = light_count_buffer.Load(rc.lights.mDispatchSize.x * group_index.x + group_index.y);
    uint index_offset = rc.lights.mDispatchSize.x * LIGHT_CULL_MAX_LIGHTS * group_index.y + group_index.x * LIGHT_CULL_MAX_LIGHTS;
    
    // evaluate Point and Spot lights
    for (int light_idx = 0; light_idx < fc.mNrOfLights; light_idx++)
    //for (uint light_idx = 0; light_idx < light_count; light_idx++)
    {
        RTLight light = lights[light_idx];
        //RTLight light = lights[light_index_buffer.Load(index_offset + light_idx)];
        float dist_to_light = length(light.mPosition.xyz - inParams.ws_position.xyz);

        switch (light.mType)
        {
            case RT_LIGHT_TYPE_POINT:
            {
                    float3 Wi = SamplePointLight(light, inParams.ws_position.xyz);
                    total_radiance += EvaluatePointLight(surface, light, Wi, Wo, dist_to_light);
                }
                break;

            case RT_LIGHT_TYPE_SPOT:
            {
                    float3 Wi = SampleSpotLight(light, inParams.ws_position.xyz);
                    total_radiance += EvaluateSpotLight(surface, light, Wi, Wo, dist_to_light);
                }
                break;
        }
    }
    
    total_radiance += ComputeIBL(surface, Wo);
    
    output.color = float4(total_radiance, 1.0);
    
    return output;
}