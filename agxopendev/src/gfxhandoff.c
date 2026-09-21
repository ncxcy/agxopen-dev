#include "gfxhandoff.h"

static void agx_handoff_relax(struct agx_handoff *ho)
{
	if (ho->ops->relax)
		ho->ops->relax(ho->ctx);
}

static void agx_handoff_barrier(struct agx_handoff *ho)
{
	if (ho->ops->barrier)
		ho->ops->barrier(ho->ctx);
}

void agx_handoff_init(struct agx_handoff *ho, const struct agx_handoff_ops *ops,
		      void *ctx)
{
	ho->ops = ops;
	ho->ctx = ctx;
	ho->locked = false;
	ho->initialized = false;
}

int agx_handoff_lock(struct agx_handoff *ho, u32 max_spins)
{
	u32 spins = 0;

	ho->ops->write8(ho->ctx, AGX_HANDOFF_OFF_LOCK_AP, 1);
	agx_handoff_barrier(ho);

	while (ho->ops->read8(ho->ctx, AGX_HANDOFF_OFF_LOCK_FW) != 0) {
		if (ho->ops->read32(ho->ctx, AGX_HANDOFF_OFF_TURN) != 0) {
			ho->ops->write8(ho->ctx, AGX_HANDOFF_OFF_LOCK_AP, 0);
			agx_handoff_barrier(ho);
			while (ho->ops->read32(ho->ctx, AGX_HANDOFF_OFF_TURN) !=
			       0) {
				if (max_spins && ++spins > max_spins)
					return AGX_ETIMEDOUT;
				agx_handoff_relax(ho);
			}
			ho->ops->write8(ho->ctx, AGX_HANDOFF_OFF_LOCK_AP, 1);
			agx_handoff_barrier(ho);
		}
		if (max_spins && ++spins > max_spins) {
			ho->ops->write8(ho->ctx, AGX_HANDOFF_OFF_LOCK_AP, 0);
			agx_handoff_barrier(ho);
			return AGX_ETIMEDOUT;
		}
		agx_handoff_relax(ho);
	}

	agx_handoff_barrier(ho);
	ho->locked = true;
	return 0;
}

void agx_handoff_unlock(struct agx_handoff *ho)
{
	agx_handoff_barrier(ho);
	ho->ops->write32(ho->ctx, AGX_HANDOFF_OFF_TURN, 1);
	agx_handoff_barrier(ho);
	ho->ops->write8(ho->ctx, AGX_HANDOFF_OFF_LOCK_AP, 0);
	agx_handoff_barrier(ho);
	ho->locked = false;
}

int agx_handoff_wait_firmware(struct agx_handoff *ho, u32 max_spins)
{
	u32 spins = 0;
	u32 slot;
	int ret;

	ho->ops->write64(ho->ctx, AGX_HANDOFF_OFF_MAGIC_AP, AGX_HANDOFF_MAGIC);

	ret = agx_handoff_lock(ho, max_spins);
	if (ret)
		return ret;

	while (ho->ops->read64(ho->ctx, AGX_HANDOFF_OFF_MAGIC_FW) !=
	       AGX_HANDOFF_MAGIC) {
		if (max_spins && ++spins > max_spins) {
			agx_handoff_unlock(ho);
			return AGX_ETIMEDOUT;
		}
		agx_handoff_relax(ho);
	}

	agx_handoff_unlock(ho);

	for (slot = 0; slot < AGX_HANDOFF_FLUSH_SLOTS; slot++) {
		ho->ops->write64(ho->ctx, AGX_HANDOFF_OFF_FLUSH_STATE(slot), 0);
		ho->ops->write64(ho->ctx, AGX_HANDOFF_OFF_FLUSH_ADDR(slot), 0);
		ho->ops->write64(ho->ctx, AGX_HANDOFF_OFF_FLUSH_SIZE(slot), 0);
	}

	ho->initialized = true;
	return 0;
}

int agx_handoff_prepare_cacheflush(struct agx_handoff *ho, u32 context,
				   u64 base, u64 size)
{
	if (context >= AGX_HANDOFF_FLUSH_SLOTS)
		return AGX_EINVAL;
	if (ho->ops->read64(ho->ctx, AGX_HANDOFF_OFF_FLUSH_STATE(context)) != 0)
		return AGX_EINVAL;

	ho->ops->write64(ho->ctx, AGX_HANDOFF_OFF_FLUSH_ADDR(context), base);
	ho->ops->write64(ho->ctx, AGX_HANDOFF_OFF_FLUSH_SIZE(context), size);
	ho->ops->write64(ho->ctx, AGX_HANDOFF_OFF_FLUSH_STATE(context),
			 AGX_HANDOFF_FLUSH_BUSY);
	return 0;
}

int agx_handoff_wait_cacheflush(struct agx_handoff *ho, u32 context,
				u32 max_spins)
{
	u32 spins = 0;

	if (context >= AGX_HANDOFF_FLUSH_SLOTS)
		return AGX_EINVAL;

	while (ho->ops->read64(ho->ctx, AGX_HANDOFF_OFF_FLUSH_STATE(context)) ==
	       AGX_HANDOFF_FLUSH_BUSY) {
		if (max_spins && ++spins > max_spins)
			return AGX_ETIMEDOUT;
		agx_handoff_relax(ho);
	}
	return 0;
}

int agx_handoff_complete_cacheflush(struct agx_handoff *ho, u32 context)
{
	if (context >= AGX_HANDOFF_FLUSH_SLOTS)
		return AGX_EINVAL;
	if (ho->ops->read64(ho->ctx, AGX_HANDOFF_OFF_FLUSH_STATE(context)) !=
	    AGX_HANDOFF_FLUSH_DONE)
		return AGX_EINVAL;

	ho->ops->write64(ho->ctx, AGX_HANDOFF_OFF_FLUSH_STATE(context), 0);
	return 0;
}

int agx_handoff_prepare_unmap(struct agx_handoff *ho, u32 context, u64 base,
			     u64 size)
{
	if (context >= AGX_HANDOFF_FLUSH_SLOTS)
		return AGX_EINVAL;
	if (ho->ops->read64(ho->ctx, AGX_HANDOFF_OFF_FLUSH_STATE(context)) != 0)
		return AGX_EINVAL;

	ho->ops->write64(ho->ctx, AGX_HANDOFF_OFF_FLUSH_ADDR(context),
			 AGX_HANDOFF_UNMAP_TAG |
				 (base & AGX_HANDOFF_UNMAP_ADDR_MASK));
	ho->ops->write64(ho->ctx, AGX_HANDOFF_OFF_FLUSH_SIZE(context), size);
	ho->ops->write64(ho->ctx, AGX_HANDOFF_OFF_FLUSH_STATE(context),
			 AGX_HANDOFF_FLUSH_DONE);
	return 0;
}

int agx_handoff_complete_unmap(struct agx_handoff *ho, u32 context)
{
	if (context >= AGX_HANDOFF_FLUSH_SLOTS)
		return AGX_EINVAL;
	if (ho->ops->read64(ho->ctx, AGX_HANDOFF_OFF_FLUSH_STATE(context)) !=
	    AGX_HANDOFF_FLUSH_DONE)
		return AGX_EINVAL;

	ho->ops->write64(ho->ctx, AGX_HANDOFF_OFF_FLUSH_STATE(context), 0);
	return 0;
}
