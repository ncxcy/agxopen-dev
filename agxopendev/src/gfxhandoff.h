#ifndef AGX_GFXHANDOFF_H
#define AGX_GFXHANDOFF_H

#include "agx/types.h"
#include "agx/uat.h"

struct agx_handoff_ops {
	u8 (*read8)(void *ctx, u32 offset);
	void (*write8)(void *ctx, u32 offset, u8 value);
	u32 (*read32)(void *ctx, u32 offset);
	void (*write32)(void *ctx, u32 offset, u32 value);
	u64 (*read64)(void *ctx, u32 offset);
	void (*write64)(void *ctx, u32 offset, u64 value);
	void (*relax)(void *ctx);
	void (*barrier)(void *ctx);
};

struct agx_handoff {
	const struct agx_handoff_ops *ops;
	void *ctx;
	bool locked;
	bool initialized;
};

void agx_handoff_init(struct agx_handoff *ho, const struct agx_handoff_ops *ops,
		      void *ctx);
int agx_handoff_wait_firmware(struct agx_handoff *ho, u32 max_spins);
int agx_handoff_lock(struct agx_handoff *ho, u32 max_spins);
void agx_handoff_unlock(struct agx_handoff *ho);
int agx_handoff_prepare_cacheflush(struct agx_handoff *ho, u32 context,
				   u64 base, u64 size);
int agx_handoff_wait_cacheflush(struct agx_handoff *ho, u32 context,
				u32 max_spins);
int agx_handoff_complete_cacheflush(struct agx_handoff *ho, u32 context);
int agx_handoff_prepare_unmap(struct agx_handoff *ho, u32 context, u64 base,
			     u64 size);
int agx_handoff_complete_unmap(struct agx_handoff *ho, u32 context);

#endif
