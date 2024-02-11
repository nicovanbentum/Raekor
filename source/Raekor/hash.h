#pragma once

#include <cstdint>

namespace Raekor {

constexpr uint32_t val_32_const = 0x811c9dc5;
constexpr uint32_t prime_32_const = 0x1000193;
constexpr uint64_t val_64_const = 0xcbf29ce484222325;
constexpr uint64_t prime_64_const = 0x100000001b3;


inline constexpr uint32_t gHash32Bit(const char* const str, const uint32_t value = val_32_const) noexcept
{
    return ( str[0] == '\0' ) ? value : gHash32Bit(&str[1], ( value ^ uint32_t(str[0]) ) * prime_32_const);
}


inline constexpr uint64_t gHash64Bit(const char* const str, const uint64_t value = val_64_const) noexcept
{
    return ( str[0] == '\0' ) ? value : gHash64Bit(&str[1], ( value ^ uint64_t(str[0]) ) * prime_64_const);
}


inline uint64_t gHashFNV1a(const char* const inData, uint64_t inLength)
{
    auto hash = val_64_const;

    for (uint32_t i = 0; i < inLength; i++)
        hash = ( hash ^ inData[i] ) * prime_64_const;

    return hash;
}

} // raekor