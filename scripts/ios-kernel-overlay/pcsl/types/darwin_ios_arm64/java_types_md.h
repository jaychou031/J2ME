/*
 * PCSL 平台定义：arm64 / iOS（替代补丁里的 linux_mips_gcc.gmk）
 *
 * 关键差异：MIPS32 是 32 位指针，arm64 是 64 位。ROMImage 必须由
 * romizer 在 64 位配置下重新生成，不能复用仓库里 MIPS 的 ROMImage.o。
 */
#if !defined _JAVA_TYPES_H_
# error "Never include <java_types_md.h> directly; use <java_types.h> instead."
#endif

#ifndef _JAVA_TYPES_MD_H_
#define _JAVA_TYPES_MD_H_

#ifndef _JAVASOFT_JNI_H_
typedef signed char         jbyte;
typedef unsigned short      jchar;
typedef int                 jint;      /* arm64 上仍是 32 位 */
typedef long long           jlong;     /* 64 位 */
#endif

#define PCSL_LLD "%lld"

#endif
