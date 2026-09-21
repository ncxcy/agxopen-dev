#ifndef AGX_GPU_EP_H
#define AGX_GPU_EP_H

#include "agx/types.h"

#define AGX_GPU_MSG_TYPE_SHIFT 48
#define AGX_GPU_MSG_TYPE_MASK 0xffffULL

#define AGX_GPU_MSG_EVENT 0x42u
#define AGX_GPU_MSG_INIT 0x81u
#define AGX_GPU_MSG_DOORBELL 0x83u
#define AGX_GPU_MSG_FWCTL 0x84u
#define AGX_GPU_MSG_STOP 0x85u
#define AGX_GPU_MSG_DOORBELL_ALT 0x86u

#define AGX_GPU_EVENT_TYPE_MASK 0x3fULL
#define AGX_GPU_EVENT_TYPE_VALUE 0x2ULL
#define AGX_GPU_KICK_INDEX_MASK 0x7u
#define AGX_GPU_KICK_INDEX_SHIFT 2

#define AGX_GPU_INITDATA_MASK 0xfffffffffffULL
#define AGX_GPU_DOORBELL_CHANNEL_MASK 0xffffULL
#define AGX_GPU_DOORBELL_KICK 0x10u

static inline u32 agx_gpu_msg_type(u64 message)
{
	return (u32)AGX_FIELD_GET(message, AGX_GPU_MSG_TYPE_SHIFT,
				  AGX_GPU_MSG_TYPE_MASK);
}

static inline u64 agx_gpu_msg_init(u64 initdata_addr)
{
	return AGX_FIELD_PUT(AGX_GPU_MSG_INIT, AGX_GPU_MSG_TYPE_SHIFT,
			     AGX_GPU_MSG_TYPE_MASK) |
	       (initdata_addr & AGX_GPU_INITDATA_MASK);
}

static inline u64 agx_gpu_msg_doorbell(u32 channel)
{
	return AGX_FIELD_PUT(AGX_GPU_MSG_DOORBELL, AGX_GPU_MSG_TYPE_SHIFT,
			     AGX_GPU_MSG_TYPE_MASK) |
	       (u64)(channel & AGX_GPU_DOORBELL_CHANNEL_MASK);
}

static inline u64 agx_gpu_msg_kick(u32 index, bool alt)
{
	u32 type = alt ? AGX_GPU_MSG_DOORBELL_ALT : AGX_GPU_MSG_DOORBELL;

	return AGX_FIELD_PUT(type, AGX_GPU_MSG_TYPE_SHIFT,
			     AGX_GPU_MSG_TYPE_MASK) |
	       (u64)((index & AGX_GPU_KICK_INDEX_MASK)
		     << AGX_GPU_KICK_INDEX_SHIFT);
}

static inline u64 agx_gpu_msg_stop(u64 counter)
{
	return AGX_FIELD_PUT(AGX_GPU_MSG_STOP, AGX_GPU_MSG_TYPE_SHIFT,
			     AGX_GPU_MSG_TYPE_MASK) |
	       (counter & AGX_GPU_INITDATA_MASK);
}

static inline bool agx_gpu_msg_is_event(u64 message)
{
	return AGX_FIELD_GET(message, AGX_GPU_MSG_TYPE_SHIFT,
			     AGX_GPU_EVENT_TYPE_MASK) ==
	       AGX_GPU_EVENT_TYPE_VALUE;
}

static inline u64 agx_gpu_msg_fwctl(void)
{
	return AGX_FIELD_PUT(AGX_GPU_MSG_FWCTL, AGX_GPU_MSG_TYPE_SHIFT,
			     AGX_GPU_MSG_TYPE_MASK);
}

static inline u64 agx_gpu_msg_initdata(u64 message)
{
	return message & AGX_GPU_INITDATA_MASK;
}

static inline u32 agx_gpu_msg_channel(u64 message)
{
	return (u32)(message & AGX_GPU_DOORBELL_CHANNEL_MASK);
}

#endif
