#version 460 core
#extension GL_GOOGLE_include_directive : require

#include "include/structs.glsl"

//#extension GL_ARB_bindless_texture : require

// TODO: optimize by packing more data per byte
// MRT texture output 
// NOTE: always render to 1, 2 or 4 component vectors
layout(location = 0) out vec4 RT_Normals;
layout(location = 1) out vec4 RT_Albedo;
layout(location = 2) out vec4 RT_MetallicRoughness;
layout(location = 3) out vec4 RT_EntityID;
layout(location = 4) out vec4 RT_Velocity;

// constant mesh values
layout(binding = 1) uniform sampler2D albedoTexture;
layout(binding = 2) uniform sampler2D normalTexture;
layout(binding = 3) uniform sampler2D metallicTexture;
layout(binding = 4) uniform sampler2D roughnessTexture;

layout(location = 0) in VS_OUT {
    vec2 uv;
    mat3 TBN;
    vec4 prevPos;
    vec4 currentPos;
} vs_out;

layout(binding = 0) uniform ubo {
    mat4 prevViewProj;
    mat4 projection;
    mat4 view;
    mat4 model;
    vec4 colour;
    vec2 jitter;
    vec2 prevJitter;
    float metallic;
    float roughness;
    uint entity;
    float mLODFade;
};

float InterleavedGradientNoise(vec2 pixel) {
  vec3 magic = vec3(0.06711056f, 0.00583715f, 52.9829189f);
  return fract(magic.z * fract(dot(pixel, magic.xy)));
}

// From UE4's MaterialTemplate.ush
void  ClipLODTransition(vec2 SvPosition,  float DitherFactor) {
     if  (abs(DitherFactor) > 0.001) {
        float  RandCos = cos(dot(floor(SvPosition.xy), vec2( 347.83451793 , 3343.28371963 )));
        //float  RandomVal = fract(RandCos *  1000.0 );
        float RandomVal = InterleavedGradientNoise(SvPosition);
        bool  RetVal = (DitherFactor <  0.0 ) ? (DitherFactor  +  1.0  >  RandomVal) : (DitherFactor < RandomVal);


        if ((float(RetVal) - 0.001) < 0)
            discard;
    }
}

void main() {
    vec4 color = texture(albedoTexture, vs_out.uv);
    if(color.a < 0.5) discard;

    ClipLODTransition(gl_FragCoord.xy, mLODFade);

	// write the color to the color texture of the gbuffer
	RT_Albedo = color * colour;

	// retrieve the normal from the normal map
    vec4 sampledNormal = texture(normalTexture, vs_out.uv);
	vec3 glNormal = sampledNormal.xyz * 2.0 - 1.0;
    vec3 normal = vs_out.TBN * glNormal;
	RT_Normals = vec4(normal, 1.0);

    float sampled_metallic = texture(metallicTexture, vs_out.uv).b;
    float sampled_roughness = texture(roughnessTexture, vs_out.uv).g;
    RT_MetallicRoughness = vec4(0.0f, sampled_roughness * roughness, sampled_metallic * metallic, 1.0);

    vec3 prevPosNDC = (vs_out.prevPos.xyz / vs_out.prevPos.w);
    vec3 currentPosNDC = (vs_out.currentPos.xyz / vs_out.currentPos.w);
    
    RT_Velocity = vec4(currentPosNDC.xy - prevPosNDC.xy, 0.0, 1.0);

    RT_EntityID = vec4(entity, 0, 0, 1.0);
}