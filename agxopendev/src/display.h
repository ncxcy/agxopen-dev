#ifndef AGX_DISPLAY_H
#define AGX_DISPLAY_H

#include <linux/list.h>
#include <linux/spinlock.h>
#include "agx/power.h"

typedef void (*agx_display_callback)(void *cookie, enum agx_power_state from,
				     enum agx_power_state to);

struct agx_display_listener {
	agx_display_callback callback;
	void *cookie;
	struct list_head node;
};

struct agx_display_broker {
	struct list_head listeners;
	spinlock_t lock;
};

void agx_display_broker_init(struct agx_display_broker *broker);
void agx_display_register(struct agx_display_broker *broker,
			  struct agx_display_listener *listener);
void agx_display_unregister(struct agx_display_broker *broker,
			    struct agx_display_listener *listener);
void agx_display_notify(struct agx_display_broker *broker,
			enum agx_power_state from, enum agx_power_state to);

#endif
