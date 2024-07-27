#ifndef PACKING_HLSL
#define PACKING_HLSL

uint UInt2ToUInt(uint x, uint y)
{
    return (x << 16) | (y & 0xFFFF);
}

uint2 UIntToUInt2(uint val)
{
    return uint2(val >> 16, val & 0xFFFF);
}

float4 RGBA8ToFloat4(uint val) {
    float4 unpacked = float4(0.0, 0.0, 0.0, 0.0);
    unpacked.x = val & 255;
    unpacked.y = (val >> 8) & 255;
    unpacked.z = (val >> 16) & 255;
    unpacked.w = (val >> 24) & 255;
    return unpacked / 255;
}

uint Float4ToRGBA8(float4 val) {
    uint packed = 0;
    packed += uint(val.r * 255);
    packed += uint(val.g * 255) << 8;
    packed += uint(val.b * 255) << 16;
    packed += uint(val.a * 255) << 24;
    return packed;
}

uint Float16x2ToUint(float2 val) {
    uint packed = 0;
    packed += uint(val.x * 65535);
    packed += uint(val.y * 65535) << 16;
    return packed;
}

float2 UintToFloat16x2(uint val) {
    float2 unpacked = float2(0.0, 0.0);
    unpacked.x = val & 65535;
    unpacked.y = (val >> 16) & 65535;
    return unpacked / 65535;
}

uint Float3ToUint(float3 val) {
    uint packed = 0;
    uint bitmask10 = (1 << 10) - 1;
    uint bitmask11 = (1 << 11) - 1;
    packed += uint(val.x * bitmask11);
    packed += uint(val.y * bitmask11) << 11;
    packed += uint(val.z * bitmask10) << 22;
    return packed;
}

float3 UintToFloat3(uint val) {
    float3 unpacked = float3(0.0, 0.0, 0.0);
    uint bitmask10 = (1 << 10) - 1;
    uint bitmask11 = (1 << 11) - 1;
    unpacked.x = (val & bitmask11);
    unpacked.y = ((val >> 11) & bitmask11);
    unpacked.z = ((val >> 22) & bitmask10);
    return unpacked / uint3(bitmask11, bitmask11, bitmask10);
}

#define RGB9E5_EXPONENT_BITS          5
#define RGB9E5_MANTISSA_BITS          9
#define RGB9E5_EXP_BIAS               15
#define RGB9E5_MAX_VALID_BIASED_EXP   31

#define MAX_RGB9E5_EXP               (RGB9E5_MAX_VALID_BIASED_EXP - RGB9E5_EXP_BIAS)
#define RGB9E5_MANTISSA_VALUES       (1<<RGB9E5_MANTISSA_BITS)
#define MAX_RGB9E5_MANTISSA          (RGB9E5_MANTISSA_VALUES-1)
#define MAX_RGB9E5                   ((float(MAX_RGB9E5_MANTISSA))/RGB9E5_MANTISSA_VALUES * (1<<MAX_RGB9E5_EXP))
#define EPSILON_RGB9E5               ((1.0/RGB9E5_MANTISSA_VALUES) / (1<<RGB9E5_EXP_BIAS))

float ClampRangeForRGB9E5(float x)
{
    return clamp(x, 0.0, MAX_RGB9E5);
}

int FloorLog2(float x)
{
    uint f = asuint(x);
    uint biasedexponent = (f & 0x7F800000u) >> 23;
    return int(biasedexponent) - 127;
}


// https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_shared_exponent.txt
uint Float3ToRGB9E5(float3 rgb)
{
    float rc = ClampRangeForRGB9E5(rgb.x);
    float gc = ClampRangeForRGB9E5(rgb.y);
    float bc = ClampRangeForRGB9E5(rgb.z);

    float maxrgb = max(rc, max(gc, bc));
    int exp_shared = max(-RGB9E5_EXP_BIAS - 1, FloorLog2(maxrgb)) + 1 + RGB9E5_EXP_BIAS;
    float denom = exp2(float(exp_shared - RGB9E5_EXP_BIAS - RGB9E5_MANTISSA_BITS));

    int maxm = int(floor(maxrgb / denom + 0.5));
    if (maxm == MAX_RGB9E5_MANTISSA + 1)
    {
        denom *= 2;
        exp_shared += 1;
    }

    int rm = int(floor(rc / denom + 0.5));
    int gm = int(floor(gc / denom + 0.5));
    int bm = int(floor(bc / denom + 0.5));

    return (uint(rm) << (32 - 9))
        | (uint(gm) << (32 - 9 * 2))
        | (uint(bm) << (32 - 9 * 3))
        | uint(exp_shared);
}

uint BitfieldExtract(uint value, uint offset, uint bits)
{
    uint mask = (1u << bits) - 1u;
    return (value >> offset) & mask;
}

float3 RGB9E5ToFloat3(uint v)
{
    int exponent =
        int(BitfieldExtract(v, 0, RGB9E5_EXPONENT_BITS)) - RGB9E5_EXP_BIAS - RGB9E5_MANTISSA_BITS;
    float scale = exp2(float(exponent));

    return float3(
        float(BitfieldExtract(v, 32 - RGB9E5_MANTISSA_BITS, RGB9E5_MANTISSA_BITS)) * scale,
        float(BitfieldExtract(v, 32 - RGB9E5_MANTISSA_BITS * 2, RGB9E5_MANTISSA_BITS)) * scale,
        float(BitfieldExtract(v, 32 - RGB9E5_MANTISSA_BITS * 3, RGB9E5_MANTISSA_BITS)) * scale
    );
}

float4 UnpackAlbedo(uint4 inPacked) 
{
    return RGBA8ToFloat4(inPacked.x);
}

float3 UnpackNormal(uint4 inPacked) {
    return UintToFloat3(inPacked.y) * 2.0 - 1.0;
}

void UnpackMetallicRoughness(uint4 inPacked, out float metalness, out float roughness) 
{
    float2 mr = UintToFloat16x2(inPacked.z);
    metalness = mr.r;
    roughness = mr.g;
}

float3 UnpackEmissive(uint4 inPacked)
{
    return RGB9E5ToFloat3(inPacked.a);
}

void PackAlbedo(float3 inAlbedo, inout uint4 ioPacked) 
{
    float4 albedo = UnpackAlbedo(ioPacked);
    albedo.rgb = inAlbedo;
    ioPacked.x = Float4ToRGBA8(albedo);
}

void PackAlpha(float inAlpha, inout uint4 ioPacked) 
{
    float4 albedo = UnpackAlbedo(ioPacked);
    albedo.a = inAlpha;
    ioPacked.x = Float4ToRGBA8(albedo);
}

void PackNormal(float3 inNormal, inout uint4 ioPacked) 
{
    ioPacked.y = Float3ToUint(normalize(inNormal) * 0.5 + 0.5);
}

void PackMetallicRoughness(float metalness, float roughness, inout uint4 ioPacked) 
{
    ioPacked.z = Float16x2ToUint(float2(metalness, roughness));
}


void PackEmissive(float3 inEmissive, inout uint4 ioPacked)
{
    ioPacked.w = Float3ToRGB9E5(inEmissive);
}


void PackGBuffer(float4 inAlbedo, float3 inNormal, float3 inEmissive, float inMetallic, float inRoughness, inout uint4 ioPacked)
{
    PackAlbedo(inAlbedo.rgb, ioPacked);
    PackAlpha(inAlbedo.a, ioPacked);
    PackNormal(inNormal, ioPacked);
    PackMetallicRoughness(inMetallic, inRoughness, ioPacked);
}

void PackMetallic(float metalness, inout uint4 ioPacked) 
{
    float temp_metallic, temp_roughness;
    UnpackMetallicRoughness(ioPacked, temp_metallic, temp_roughness);
    ioPacked.z = Float16x2ToUint(float2(metalness, temp_roughness));
}

void PackRoughness(float roughness, inout uint4 ioPacked) 
{
    float temp_metallic, temp_roughness;
    UnpackMetallicRoughness(ioPacked, temp_metallic, temp_roughness);
    ioPacked.z = Float16x2ToUint(float2(temp_metallic, roughness));
}

#endif