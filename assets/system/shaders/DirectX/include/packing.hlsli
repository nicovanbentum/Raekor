#ifndef PACKING_HLSL
#define PACKING_HLSL

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
    packed += uint(val.x * 255);
    packed += uint(val.g * 255) << 8;
    packed += uint(val.b * 255) << 16;
    packed += uint(val.a * 255) << 24;
    return packed;
}

#endif