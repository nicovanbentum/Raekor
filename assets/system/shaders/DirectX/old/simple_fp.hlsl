struct PS_INPUT {
    float4 pos : SV_POSITION;
    float3 wpos : WORLDPOS;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BINORMAL;
    float2 uv : TEXCOORD;
};

Texture2D gAlbedoTexture      : register(t0);
Texture2D gNormalTexture      : register(t1);
Texture2D gMetalRoughTexture  : register(t2);

SamplerState SamplerAnisoWrap : register(s0);

cbuffer Constants : register(b0) {
    float mLODFade;
    float pad0;
    float mMetallic;
    float mRoughness;
    float4 mAlbedo;
    float4 mLightPosition;
    float4 mCameraPosition;
    float4x4 mModelMatrix;
    float4x4 mViewMatrix;
    float4x4 mProjectionMatrix;
}

float3 CorrectGamma(float3 L) {
    return pow(L, (1.0 / 2.2).xxx);
}

float InterleavedGradientNoise(float2 pixel) {
  float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
  return frac(magic.z * frac(dot(pixel, magic.xy)));
}

// From UE4's MaterialTemplate.ush
void  ClipLODTransition(float2 SvPosition,  float DitherFactor) {
     if  (abs(DitherFactor) > 0.001) {
        float  RandCos = cos(dot(floor(SvPosition.xy), float2( 347.83451793 , 3343.28371963 )));
        //float  RandomVal = frac(RandCos *  1000.0 );
        float RandomVal = InterleavedGradientNoise(SvPosition);
        bool  RetVal = (DitherFactor <  0.0 ) ? (DitherFactor  +  1.0  >  RandomVal) : (DitherFactor < RandomVal);

        clip(float(RetVal) - 0.001);
    }
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 sampled_albedo = gAlbedoTexture.Sample(SamplerAnisoWrap, input.uv);
    float4 sampled_normal = gNormalTexture.Sample(SamplerAnisoWrap, input.uv);
    float4 sampled_material = gMetalRoughTexture.Sample(SamplerAnisoWrap, input.uv);

    float4 albedo = sampled_albedo * mAlbedo;

    float3x3 TBN = transpose(float3x3(input.tangent, input.bitangent, input.normal));
    float3 normal = normalize(mul(TBN, sampled_normal.xyz * 2.0 - 1.0));
    // normal = normalize(input.normal);
    
    float constant = 1.0f;
    float linear_falloff = 0.045;
    float quadratic_falloff = 0.0075;
    float3 light_color = float3(1.0, 0.7725, 0.56);

    float ambient_strength = 0.1f;
    float3 ambient = ambient_strength * albedo.rgb;
    
    // diffuse
    float3 light_dir = normalize(mLightPosition.xyz - input.wpos.xyz);
    float diff = clamp(dot(normal, light_dir), 0, 1);
	// TODO: figure out why with diff it looks so weird?
    float3 diffuse = light_color * diff * albedo.rgb;

	// specular
    float3 view_dir = normalize(mCameraPosition.xyz - input.wpos.xyz);
    float3 reflect_dir = reflect(-light_dir, normal);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0f);
    float3 specular = (1.0 - sampled_material.g) * spec * albedo.rgb;
    
    // distance between the light and the vertex
    float distance = length(mLightPosition.xyz - input.wpos.xyz);
    float attenuation = 1.0 / (constant + linear_falloff * distance + quadratic_falloff * (distance * distance));
	
    ambient = ambient * attenuation;
    diffuse = diffuse * attenuation;
    specular = specular * attenuation;

    float3 L = ambient + diffuse + specular;
    
    float3 final_color = L;
    final_color = CorrectGamma(L);

    return float4(final_color, albedo.a);
}