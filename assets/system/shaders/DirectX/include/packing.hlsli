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

void PackAlbedo(float4 inAlbedo, inout uint4 ioPacked) {
    ioPacked.x = Float4ToRGBA8(inAlbedo);
}

void PackNormal(float3 inNormal, inout uint4 ioPacked) {
    ioPacked.y = Float3ToUint(normalize(inNormal) * 0.5 + 0.5);
}

void PackMetallicRoughness(float metalness, float roughness, inout uint4 ioPacked) {
    ioPacked.z = Float16x2ToUint(float2(metalness, roughness));
}

void PackGBuffer(float4 inAlbedo, float3 inNormal, float3 inEmissive, float inMetallic, float inRoughness, inout uint4 ioPacked)
{
    PackAlbedo(inAlbedo, ioPacked);
    PackNormal(inNormal, ioPacked);
    PackMetallicRoughness(inMetallic, inRoughness, ioPacked);
}

float4 UnpackAlbedo(uint4 inPacked) {
    return RGBA8ToFloat4(inPacked.x);
}

float3 UnpackNormal(uint4 inPacked) {
    return UintToFloat3(inPacked.y) * 2.0 - 1.0;
}

void UnpackMetallicRoughness(uint4 inPacked, out float metalness, out float roughness) {
    float2 mr = UintToFloat16x2(inPacked.z);
    metalness = mr.r;
    roughness = mr.g;
}

#endif