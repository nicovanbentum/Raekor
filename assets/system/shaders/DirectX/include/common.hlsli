 #ifndef ROOT_SIG_HLSLI
 #define ROOT_SIG_HLSLI

 //#define GLOBAL_ROOT_SIGNATURE "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED )," \
 //                            "RootConstants(num32BitConstants=32, b0), " \
 //                            "StaticSampler(s0, addressU = TEXTURE_ADDRESS_CLAMP, filter = FILTER_ANISOTROPIC)"

#define ROOT_CONSTANTS(T, name) ConstantBuffer<T> name : register(b0, space0);

SamplerState SamplerPointWrap       : register(s0);
SamplerState SamplerPointClamp      : register(s1);
SamplerState SamplerLinearWrap      : register(s2);
SamplerState SamplerLinearClamp     : register(s3);
SamplerState SamplerAnisoWrap       : register(s4);
SamplerState SamplerAnisoClamp      : register(s5);

#endif
