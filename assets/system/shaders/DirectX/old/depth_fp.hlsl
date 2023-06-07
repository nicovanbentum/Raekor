struct PS_INPUT {
    float4 pos : SV_POSITION;
    float3 wpos : WORLDPOS;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BINORMAL;
    float2 uv : TEXCOORD;
};

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

void main(PS_INPUT input)
{
    ClipLODTransition(input.pos.xy, mLODFade);
}