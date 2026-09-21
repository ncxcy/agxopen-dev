#ifndef KSHIM_IOPOLL_H
#define KSHIM_IOPOLL_H
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <linux/io.h>
static inline long long kshim_now_us(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}
#define readl_poll_timeout(addr, val, cond, sleep_us, timeout_us)          \
	({                                                                 \
		long long kshim_end = kshim_now_us() + (long long)(timeout_us); \
		int kshim_r = 0;                                           \
		for (;;) {                                                 \
			(val) = readl(addr);                               \
			if (cond)                                          \
				break;                                     \
			if (kshim_now_us() >= kshim_end) {                 \
				(val) = readl(addr);                       \
				kshim_r = (cond) ? 0 : -ETIMEDOUT;         \
				break;                                     \
			}                                                  \
			usleep(sleep_us);                                  \
		}                                                          \
		kshim_r;                                                   \
	})
#endif
