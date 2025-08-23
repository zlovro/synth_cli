//
// Created by Made on 08/03/2025.
//

#ifndef TYPES_H
#define TYPES_H

#define DEBUG

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __arm__
#include "arm_math.h"
#endif

#define roundUpTo(x, radix) (((x + radix - 1) / radix) * radix)

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef u8 byte;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef s8 sbyte;

typedef float  f32;
typedef double f64;

typedef u32 sz;

typedef char *str;

#define clamp(x, a, b) max(a, min(b, x))
#define clamp01(x) clamp(x, 0, 1)
#define clamp11(x) clamp(x, -1, 1)

#define EDGE_INVALID 0
#define EDGE_FALLING 1
#define EDGE_RISING 2

#define assertSize(obj, sz) static_assert(sizeof(obj) == sz)
#define assertSizeAlignedTo(obj, tgt) static_assert(tgt % sizeof(obj) == 0)
#define assertAlignedTo(x, y) static_assert(x % y == 0)

#define zmem(m, s) memset(m, 0, s)

#define talloc(t) (malloc(sizeof(t)))

#ifndef STM32H743xx
#define DESKTOP
#define UNUSED(x) (void)(x)
#endif

#ifdef __GNUC__
#define pstruct struct __attribute__((packed))
#define DMA_BUFFER __attribute__((section(".dma_buffer")))
#define RAM_D2_BUFFER DMA_BUFFER
#define BDMA_BUFFER __attribute__((section(".bdma_buffer")))
#define RAM_D3_BUFFER BDMA_BUFFER
#else
#define _ALLOW_KEYWORD_MACROS
#define pstruct struct
#endif

#ifdef __cplusplus
#define enumAsFlag(e) \
    inline e operator|(e a, e b) {return (e) ((u64)a | (u64)b); } \
    inline e operator|=(e& a, e b) {return a = (a | b);} \
    inline e operator&(e a, e b) {return (e) ((u64)a & (u64)b); } \
    inline e operator&=(e& a, e b) {return a = (a & b);}
#else
#define enumAsFlag(e)
#define max(a, b) (a > b ? a : b)
#define min(a, b) (a < b ? a : b)
#endif

#define magic(a, b, c, d) ((u32)((d << 24) | (c << 16) | (b << 8) | a))

#define bswap16(n) (((n&0xFF00)>>8)|((n&0x00FF)<<8))
#define bswap24(n) ((n & 0xFF00) | ((n >> 16) & 0xFF) | ((n & 0xFF) << 16))
#define bswap32(n) ((bswap16((n&0xFFFF0000)>>16))|((bswap16(n&0x0000FFFF))<<16))
#define bswap64(n) ((bswap32((n&0xFFFFFFFF00000000)>>32))|((bswap32(n&0x00000000FFFFFFFF))<<32))

#define halt(cond) { volatile u32 __halt_ctr = 0; while (cond) { __halt_ctr++; } }

#endif //TYPES_H
