#ifndef AGX_UAT_PRIV_H
#define AGX_UAT_PRIV_H

#include "agx/types.h"
#include "agx/uat.h"

struct agx_uat_ops {
	int (*alloc_table)(void *ctx, void **cpu, u64 *phys);
	void (*free_table)(void *ctx, void *cpu, u64 phys);
	void *(*alloc_bookkeeping)(void *ctx, size_t size);
	void (*free_bookkeeping)(void *ctx, void *ptr, size_t size);
};

struct agx_uat_node {
	void *cpu;
	u64 phys;
	void **children;
	u32 child_count;
};

struct agx_uat {
	const struct agx_uat_ops *ops;
	void *ctx;
	void *l0_cpu;
	struct agx_uat_node *l1[AGX_UAT_CONTEXT_COUNT][2];
};

void agx_uat_init(struct agx_uat *uat, const struct agx_uat_ops *ops,
		  void *ctx, void *l0_cpu);
int agx_uat_map_page(struct agx_uat *uat, u32 ctx_id, u64 iova, u64 phys,
		     u64 flags);
int agx_uat_unmap_page(struct agx_uat *uat, u32 ctx_id, u64 iova);
void agx_uat_free_context(struct agx_uat *uat, u32 ctx_id);
u64 agx_uat_lookup(struct agx_uat *uat, u32 ctx_id, u64 iova, bool *valid);

#endif
