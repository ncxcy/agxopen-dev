#ifndef AGX_TYPES_H
#define AGX_TYPES_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/stddef.h>
#else
#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#endif

#ifdef __cplusplus
#define AGX_BEGIN_DECLS extern "C" {
#define AGX_END_DECLS }
#else
#define AGX_BEGIN_DECLS
#define AGX_END_DECLS
#endif

#define AGX_BIT64(n) (1ULL << (n))
#define AGX_FIELD_GET(value, shift, mask) \
	(((u64)(value) >> (shift)) & (u64)(mask))
#define AGX_FIELD_PUT(value, shift, mask) \
	(((u64)(value) & (u64)(mask)) << (shift))
#define AGX_LOAD_ACQ(x) __atomic_load_n(&(x), __ATOMIC_ACQUIRE)
#define AGX_STORE_REL(x, v) __atomic_store_n(&(x), (v), __ATOMIC_RELEASE)
#define AGX_OR_REL(x, v) __atomic_fetch_or(&(x), (v), __ATOMIC_RELEASE)

#define AGX_ASSERT_JOIN2(a, b) a##b
#define AGX_ASSERT_JOIN(a, b) AGX_ASSERT_JOIN2(a, b)
#define AGX_STATIC_ASSERT(cond) \
	typedef char AGX_ASSERT_JOIN(agx_static_assert_, __LINE__)[(cond) ? 1 : -1]

enum agx_status {
	AGX_OK = 0,
	AGX_EIO = -5,
	AGX_ENOMEM = -12,
	AGX_ENODEV = -19,
	AGX_EINVAL = -22,
	AGX_ENODATA = -61,
	AGX_ETIMEDOUT = -110
};

#endif
