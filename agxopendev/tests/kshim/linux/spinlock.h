#ifndef KSHIM_SPINLOCK_H
#define KSHIM_SPINLOCK_H
#include <pthread.h>
typedef struct {
	pthread_mutex_t m;
} spinlock_t;
static inline void spin_lock_init(spinlock_t *lock)
{
	pthread_mutex_init(&lock->m, NULL);
}
#define spin_lock_irqsave(lock, flags)        \
	do {                                  \
		(flags) = 0;                  \
		pthread_mutex_lock(&(lock)->m); \
	} while (0)
#define spin_unlock_irqrestore(lock, flags)     \
	do {                                    \
		(void)(flags);                  \
		pthread_mutex_unlock(&(lock)->m); \
	} while (0)
#endif
