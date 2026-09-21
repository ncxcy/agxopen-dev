#include "display.h"

void agx_display_broker_init(struct agx_display_broker *broker)
{
	INIT_LIST_HEAD(&broker->listeners);
	spin_lock_init(&broker->lock);
}

void agx_display_register(struct agx_display_broker *broker,
			  struct agx_display_listener *listener)
{
	unsigned long flags;

	spin_lock_irqsave(&broker->lock, flags);
	list_add_tail(&listener->node, &broker->listeners);
	spin_unlock_irqrestore(&broker->lock, flags);
}

void agx_display_unregister(struct agx_display_broker *broker,
			    struct agx_display_listener *listener)
{
	unsigned long flags;

	spin_lock_irqsave(&broker->lock, flags);
	list_del(&listener->node);
	spin_unlock_irqrestore(&broker->lock, flags);
}

void agx_display_notify(struct agx_display_broker *broker,
			enum agx_power_state from, enum agx_power_state to)
{
	struct agx_display_listener *listener;
	unsigned long flags;

	spin_lock_irqsave(&broker->lock, flags);
	list_for_each_entry(listener, &broker->listeners, node)
		listener->callback(listener->cookie, from, to);
	spin_unlock_irqrestore(&broker->lock, flags);
}
