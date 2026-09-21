#ifndef KSHIM_DELAY_H
#define KSHIM_DELAY_H
#include <unistd.h>
static inline void usleep_range(unsigned long min, unsigned long max)
{
	(void)max;
	usleep(min);
}
#endif
