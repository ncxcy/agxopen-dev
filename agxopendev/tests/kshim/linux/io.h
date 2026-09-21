#ifndef KSHIM_IO_H
#define KSHIM_IO_H
#include <linux/types.h>
#ifdef __cplusplus
extern "C" {
#endif
u32 readl(const volatile void *addr);
u64 readq(const volatile void *addr);
void writel(u32 value, volatile void *addr);
void writeq(u64 value, volatile void *addr);
#ifdef __cplusplus
}
#endif
#endif
