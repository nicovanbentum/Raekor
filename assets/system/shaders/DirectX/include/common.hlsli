 #ifndef ROOT_SIG_HLSLI
 #define ROOT_SIG_HLSLI

 //#define GLOBAL_ROOT_SIGNATURE "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED )," \
 //                            "RootConstants(num32BitConstants=32, b0), " \
 //                            "StaticSampler(s0, addressU = TEXTURE_ADDRESS_CLAMP, filter = FILTER_ANISOTROPIC)"

#define ROOT_CONSTANTS(T, name) ConstantBuffer<T> name : register(b0, space0);

#define CBV0(T, name) ConstantBuffer<T> name : register(b1, space0);
#define CBV1(T, name) ConstantBuffer<T> name : register(b2, space0);
#define CBV2(T, name) ConstantBuffer<T> name : register(b3, space0);
#define CBV3(T, name) ConstantBuffer<T> name : register(b4, space0);

#define SRV0(T, name) StructuredBuffer<T> name : register(b5, space0);
#define SRV1(T, name) StructuredBuffer<T> name : register(b6, space0);
#define SRV2(T, name) StructuredBuffer<T> name : register(b7, space0);
#define SRV3(T, name) StructuredBuffer<T> name : register(b8, space0);

#define UAV0(T, name) RWStructuredBuffer<T> name : register(b9, space0);
#define UAV1(T, name) RWStructuredBuffer<T> name : register(b10, space0);
#define UAV2(T, name) RWStructuredBuffer<T> name : register(b11, space0);
#define UAV3(T, name) RWStructuredBuffer<T> name : register(b12, space0);

SamplerState SamplerPointWrap       : register(s0);
SamplerState SamplerPointClamp      : register(s1);
SamplerState SamplerLinearWrap      : register(s2);
SamplerState SamplerLinearClamp     : register(s3);
SamplerState SamplerAnisoWrap       : register(s4);
SamplerState SamplerAnisoClamp      : register(s5);

#endif
