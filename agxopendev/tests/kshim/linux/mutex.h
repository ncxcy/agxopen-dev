#ifndef KSHIM_MUTEX_H
#define KSHIM_MUTEX_H
#include <pthread.h>
struct mutex {
	pthread_mutex_t m;
};
static inline void mutex_init(struct mutex *mutex)
{
	pthread_mutex_init(&mutex->m, NULL);
}
static inline void mutex_lock(struct mutex *mutex)
{
	pthread_mutex_lock(&mutex->m);
}
static inline void mutex_unlock(struct mutex *mutex)
{
	pthread_mutex_unlock(&mutex->m);
}
#endif
