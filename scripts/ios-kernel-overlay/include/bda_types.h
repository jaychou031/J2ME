/*
 * BBK 9588 BDA SDK 类型的最小兼容定义（仅用于编译原项目的 C 源码）。
 * iOS 上 64 位，u32/s32 仍为 32 位，与原 MIPS32 端口一致。
 */
#ifndef BDA_TYPES_H
#define BDA_TYPES_H
#include <stdint.h>
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
#endif
