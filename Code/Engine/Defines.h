#pragma once

#define ALIGN(x) __declspec(align(x))

#define RK_ASSERT(cond) assert(cond)

#ifndef TOKENPASTE
#define TOKENPASTE(x, y) x ## y
#endif

#ifndef TOKENPASTE2
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
#endif

#ifndef PRAGMA_OPTIMIZE_OFF
#define PRAGMA_OPTIMIZE_OFF __pragma( optimize( "", off ) )
#endif

#ifndef PRAGMA_OPTIMIZE_ON
#define PRAGMA_OPTIMIZE_ON __pragma( optimize( "", on ) )
#endif

#define NO_COPY_NO_MOVE(Type) \
    Type(Type&) = delete; \
    Type(Type&&) = delete; \
    Type& operator=(Type&) = delete; \
    Type& operator=(Type&&) = delete \

#define TEXTURE_SWIZZLE_RGBA 0b11'10'01'00
#define TEXTURE_SWIZZLE_RRRR 0b00'00'00'00
#define TEXTURE_SWIZZLE_GGGG 0b01'01'01'01
#define TEXTURE_SWIZZLE_BBBB 0b10'10'10'10
#define TEXTURE_SWIZZLE_AAAA 0b11'11'11'11

#define KiB(n) ((uint64_t)(n) << 10)
#define MiB(n) ((uint64_t)(n) << 20)
#define GiB(n) ((uint64_t)(n) << 30)


