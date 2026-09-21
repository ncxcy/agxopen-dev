#ifndef AGX_MAILBOX_REGS_H
#define AGX_MAILBOX_REGS_H

#include "agx/types.h"

#define AGX_MBOX_A2I_CONTROL 0x8110u
#define AGX_MBOX_I2A_CONTROL 0x8114u
#define AGX_MBOX_A2I_SEND0 0x8800u
#define AGX_MBOX_A2I_SEND1 0x8808u
#define AGX_MBOX_I2A_RECV0 0x8830u
#define AGX_MBOX_I2A_RECV1 0x8838u
#define AGX_MBOX_REGION_SIZE 0x8840u

#define AGX_MBOX_CONTROL_ENABLE 0x1u
#define AGX_MBOX_CONTROL_FULL 0x10000u
#define AGX_MBOX_CONTROL_EMPTY 0x20000u
#define AGX_MBOX_MSG1_ENDPOINT_MASK 0xffu

struct agx_mbox_message {
	u64 data;
	u32 endpoint;
	u32 flags;
};

static inline bool agx_mbox_control_full(u32 control)
{
	return (control & AGX_MBOX_CONTROL_FULL) != 0;
}

static inline bool agx_mbox_control_empty(u32 control)
{
	return (control & AGX_MBOX_CONTROL_EMPTY) != 0;
}

static inline u32 agx_mbox_control_with_enable(u32 control, bool enable)
{
	return (control & ~AGX_MBOX_CONTROL_ENABLE) |
	       (enable ? AGX_MBOX_CONTROL_ENABLE : 0u);
}

static inline u64 agx_mbox_pack_msg1(u32 endpoint)
{
	return (u64)(endpoint & AGX_MBOX_MSG1_ENDPOINT_MASK);
}

static inline struct agx_mbox_message agx_mbox_unpack(u64 msg0, u64 msg1)
{
	struct agx_mbox_message msg;

	msg.data = msg0;
	msg.endpoint = (u32)(msg1 & AGX_MBOX_MSG1_ENDPOINT_MASK);
	msg.flags = (u32)(msg1 >> 32);
	return msg;
}

#endif
