#include <linux/errno.h>
#include <linux/iopoll.h>
#include "mailbox.h"

#define AGX_MBOX_SEND_POLL_US 10
#define AGX_MBOX_SEND_TIMEOUT_US 100000

void agx_mailbox_init(struct agx_mailbox *mbox, void __iomem *base)
{
	mbox->base = base;
	mutex_init(&mbox->lock);
}

bool agx_mailbox_outbox_empty(struct agx_mailbox *mbox)
{
	return agx_mbox_control_empty(readl(mbox->base + AGX_MBOX_I2A_CONTROL));
}

void agx_mailbox_set_outbox_enable(struct agx_mailbox *mbox, bool enable)
{
	u32 control = readl(mbox->base + AGX_MBOX_I2A_CONTROL);

	writel(agx_mbox_control_with_enable(control, enable),
	       mbox->base + AGX_MBOX_I2A_CONTROL);
}

int agx_mailbox_send(struct agx_mailbox *mbox, u32 endpoint, u64 data)
{
	u32 control;
	int ret;

	mutex_lock(&mbox->lock);

	ret = readl_poll_timeout(mbox->base + AGX_MBOX_A2I_CONTROL, control,
				 !agx_mbox_control_full(control),
				 AGX_MBOX_SEND_POLL_US,
				 AGX_MBOX_SEND_TIMEOUT_US);
	if (ret) {
		mutex_unlock(&mbox->lock);
		return -ETIMEDOUT;
	}

	writeq(data, mbox->base + AGX_MBOX_A2I_SEND0);
	writeq(agx_mbox_pack_msg1(endpoint), mbox->base + AGX_MBOX_A2I_SEND1);

	mutex_unlock(&mbox->lock);

	return 0;
}

int agx_mailbox_recv(struct agx_mailbox *mbox, struct agx_mbox_message *msg)
{
	u64 msg0;
	u64 msg1;

	if (agx_mailbox_outbox_empty(mbox))
		return -ENODATA;

	msg0 = readq(mbox->base + AGX_MBOX_I2A_RECV0);
	msg1 = readq(mbox->base + AGX_MBOX_I2A_RECV1);
	*msg = agx_mbox_unpack(msg0, msg1);

	return 0;
}
