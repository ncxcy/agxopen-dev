#ifndef AGX_MAILBOX_H
#define AGX_MAILBOX_H

#include <linux/io.h>
#include <linux/mutex.h>
#include "agx/mailbox_regs.h"

struct agx_mailbox {
	void __iomem *base;
	struct mutex lock;
};

void agx_mailbox_init(struct agx_mailbox *mbox, void __iomem *base);
bool agx_mailbox_outbox_empty(struct agx_mailbox *mbox);
void agx_mailbox_set_outbox_enable(struct agx_mailbox *mbox, bool enable);
int agx_mailbox_send(struct agx_mailbox *mbox, u32 endpoint, u64 data);
int agx_mailbox_recv(struct agx_mailbox *mbox, struct agx_mbox_message *msg);

#endif
