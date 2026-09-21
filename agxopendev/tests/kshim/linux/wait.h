#ifndef KSHIM_WAIT_H
#define KSHIM_WAIT_H
#include <errno.h>
#include <pthread.h>
#include <time.h>
typedef struct {
	pthread_mutex_t m;
	pthread_cond_t c;
} wait_queue_head_t;
static inline void init_waitqueue_head(wait_queue_head_t *wq)
{
	pthread_mutex_init(&wq->m, NULL);
	pthread_cond_init(&wq->c, NULL);
}
static inline void wake_up(wait_queue_head_t *wq)
{
	pthread_mutex_lock(&wq->m);
	pthread_cond_broadcast(&wq->c);
	pthread_mutex_unlock(&wq->m);
}
static inline long msecs_to_jiffies(unsigned int ms)
{
	return (long)ms;
}
#define wait_event_timeout(wq, cond, timeout_ms)                              \
	({                                                                    \
		long kshim_ret;                                               \
		struct timespec kshim_dl;                                     \
		clock_gettime(CLOCK_REALTIME, &kshim_dl);                     \
		kshim_dl.tv_sec += (timeout_ms) / 1000;                       \
		kshim_dl.tv_nsec += ((timeout_ms) % 1000) * 1000000L;         \
		if (kshim_dl.tv_nsec >= 1000000000L) {                        \
			kshim_dl.tv_sec++;                                    \
			kshim_dl.tv_nsec -= 1000000000L;                      \
		}                                                             \
		pthread_mutex_lock(&(wq).m);                                  \
		for (;;) {                                                    \
			if (cond) {                                           \
				kshim_ret = 1;                                \
				break;                                        \
			}                                                     \
			if (pthread_cond_timedwait(&(wq).c, &(wq).m,          \
						   &kshim_dl) == ETIMEDOUT) { \
				kshim_ret = (cond) ? 1 : 0;                   \
				break;                                        \
			}                                                     \
		}                                                             \
		pthread_mutex_unlock(&(wq).m);                                \
		kshim_ret;                                                    \
	})
#endif
