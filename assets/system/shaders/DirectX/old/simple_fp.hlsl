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
    float4 mLightPosition;
    float4 mCameraPosition;
    float4x4 mModelMatrix;
    float4x4 mViewMatrix;
    float4x4 mProjectionMatrix;
}

float3 CorrectGamma(float3 L) {
    return pow(L, (1.0 / 1.8).xxx);
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 sampled_albedo = gAlbedoTexture.Sample(SamplerAnisoWrap, input.uv);
    float4 sampled_normal = gNormalTexture.Sample(SamplerAnisoWrap, input.uv);
    float4 sampled_material = gMetalRoughTexture.Sample(SamplerAnisoWrap, input.uv);
    
    float3x3 TBN = transpose(float3x3(input.tangent, input.bitangent, input.normal));
    float3 normal = normalize(mul(TBN, sampled_normal.xyz * 2.0 - 1.0));
    // normal = normalize(input.normal);
    
    float constant = 1.0f;
    float linear_falloff = 0.045;
    float quadratic_falloff = 0.0075;
    float3 light_color = float3(1.0, 0.7725, 0.56);

    float ambient_strength = 0.1f;
    float3 ambient = ambient_strength * sampled_albedo.rgb;
    
    // diffuse
    float3 light_dir = normalize(mLightPosition.xyz - input.wpos.xyz);
    float diff = clamp(dot(normal, light_dir), 0, 1);
	// TODO: figure out why with diff it looks so weird?
    float3 diffuse = light_color * diff * sampled_albedo.rgb;

	// specular
    float3 view_dir = normalize(mCameraPosition.xyz - input.wpos.xyz);
    float3 reflect_dir = reflect(-light_dir, normal);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0f);
    float3 specular = (1.0 - sampled_material.g) * spec * sampled_albedo.rgb;
    
    // distance between the light and the vertex
    float distance = length(mLightPosition.xyz - input.wpos.xyz);
    float attenuation = 1.0 / (constant + linear_falloff * distance + quadratic_falloff * (distance * distance));
	
    ambient = ambient * attenuation;
    diffuse = diffuse * attenuation;
    specular = specular * attenuation;

    float3 L = ambient + diffuse + specular;
    
    float3 final_color = L;
    final_color = CorrectGamma(L);

    return float4(final_color, sampled_albedo.a);
}