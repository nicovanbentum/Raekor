 #ifndef ROOT_SIG_HLSLI
 #define ROOT_SIG_HLSLI

 //#define GLOBAL_ROOT_SIGNATURE "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED )," \
 //                            "RootConstants(num32BitConstants=32, b0), " \
 //                            "StaticSampler(s0, addressU = TEXTURE_ADDRESS_CLAMP, filter = FILTER_ANISOTROPIC)"

#define ROOT_CONSTANTS(T, name) ConstantBuffer<T> name : register(b0, space0);

SamplerState static_sampler : register(s0);

#endif
