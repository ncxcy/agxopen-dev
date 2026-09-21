#include "uat.h"

static void agx_uat_l0_write(struct agx_uat *uat, u32 ctx_id, u32 select,
			     u64 phys, u32 asid)
{
	u64 *slot = (u64 *)uat->l0_cpu + ctx_id * 2 + select;

	AGX_STORE_REL(*slot, agx_uat_ttbr(phys, asid));
}

void agx_uat_init(struct agx_uat *uat, const struct agx_uat_ops *ops,
		  void *ctx, void *l0_cpu)
{
	u32 i;
	u32 j;

	uat->ops = ops;
	uat->ctx = ctx;
	uat->l0_cpu = l0_cpu;
	for (i = 0; i < AGX_UAT_CONTEXT_COUNT; i++)
		for (j = 0; j < 2; j++)
			uat->l1[i][j] = NULL;
}

static struct agx_uat_node *agx_uat_new_node(struct agx_uat *uat,
					     u32 child_count)
{
	struct agx_uat_node *node;

	node = uat->ops->alloc_bookkeeping(uat->ctx, sizeof(*node));
	if (!node)
		return NULL;

	if (uat->ops->alloc_table(uat->ctx, &node->cpu, &node->phys)) {
		uat->ops->free_bookkeeping(uat->ctx, node, sizeof(*node));
		return NULL;
	}

	node->child_count = child_count;
	node->children = NULL;
	if (child_count) {
		node->children = uat->ops->alloc_bookkeeping(
			uat->ctx, sizeof(void *) * child_count);
		if (!node->children) {
			uat->ops->free_table(uat->ctx, node->cpu, node->phys);
			uat->ops->free_bookkeeping(uat->ctx, node,
						   sizeof(*node));
			return NULL;
		}
	}

	return node;
}

static void agx_uat_free_node(struct agx_uat *uat, struct agx_uat_node *node,
			      u32 depth)
{
	u32 i;

	if (!node)
		return;

	if (node->children) {
		for (i = 0; i < node->child_count; i++)
			agx_uat_free_node(uat, node->children[i], depth + 1);
		uat->ops->free_bookkeeping(
			uat->ctx, node->children,
			sizeof(void *) * node->child_count);
	}

	uat->ops->free_table(uat->ctx, node->cpu, node->phys);
	uat->ops->free_bookkeeping(uat->ctx, node, sizeof(*node));
}

static struct agx_uat_node *agx_uat_l1(struct agx_uat *uat, u32 ctx_id,
				       u32 select, bool create, u32 asid)
{
	struct agx_uat_node *node = uat->l1[ctx_id][select];

	if (node || !create)
		return node;

	node = agx_uat_new_node(uat, AGX_UAT_L1_COUNT);
	if (!node)
		return NULL;

	uat->l1[ctx_id][select] = node;
	agx_uat_l0_write(uat, ctx_id, select, node->phys, asid);
	return node;
}

static struct agx_uat_node *agx_uat_child(struct agx_uat *uat,
					  struct agx_uat_node *parent, u32 idx,
					  u32 child_count, bool create)
{
	struct agx_uat_node *child = parent->children[idx];
	u64 *pte;

	if (child || !create)
		return child;

	child = agx_uat_new_node(uat, child_count);
	if (!child)
		return NULL;

	parent->children[idx] = child;
	pte = (u64 *)parent->cpu + idx;
	AGX_STORE_REL(*pte, agx_uat_table_pte(child->phys));
	return child;
}

int agx_uat_map_page(struct agx_uat *uat, u32 ctx_id, u64 iova, u64 phys,
		     u64 flags)
{
	struct agx_uat_node *l1;
	struct agx_uat_node *l2;
	struct agx_uat_node *l3;
	u64 *leaf;

	if (ctx_id >= AGX_UAT_CONTEXT_COUNT)
		return AGX_EINVAL;
	if (agx_uat_page_offset(iova) || agx_uat_page_offset(phys))
		return AGX_EINVAL;
	if (iova >= AGX_UAT_VA_LIMIT)
		return AGX_EINVAL;

	l1 = agx_uat_l1(uat, ctx_id, agx_uat_ttbr_select(iova), true, ctx_id);
	if (!l1)
		return AGX_ENOMEM;

	l2 = agx_uat_child(uat, l1, agx_uat_l1_index(iova), AGX_UAT_L2_COUNT,
			   true);
	if (!l2)
		return AGX_ENOMEM;

	l3 = agx_uat_child(uat, l2, agx_uat_l2_index(iova), AGX_UAT_L3_COUNT,
			   true);
	if (!l3)
		return AGX_ENOMEM;

	leaf = (u64 *)l3->cpu + agx_uat_l3_index(iova);
	AGX_STORE_REL(*leaf, agx_uat_pte(phys, flags | AGX_UAT_PTE_VALID));

	return 0;
}

u64 agx_uat_lookup(struct agx_uat *uat, u32 ctx_id, u64 iova, bool *valid)
{
	struct agx_uat_node *l1;
	struct agx_uat_node *l2;
	struct agx_uat_node *l3;
	u64 pte;

	*valid = false;
	if (ctx_id >= AGX_UAT_CONTEXT_COUNT || iova >= AGX_UAT_VA_LIMIT)
		return 0;

	l1 = agx_uat_l1(uat, ctx_id, agx_uat_ttbr_select(iova), false, 0);
	if (!l1)
		return 0;
	l2 = agx_uat_child(uat, l1, agx_uat_l1_index(iova), 0, false);
	if (!l2)
		return 0;
	l3 = agx_uat_child(uat, l2, agx_uat_l2_index(iova), 0, false);
	if (!l3)
		return 0;

	pte = AGX_LOAD_ACQ(((u64 *)l3->cpu)[agx_uat_l3_index(iova)]);
	*valid = (pte & AGX_UAT_PTE_VALID) != 0;
	return agx_uat_pte_phys(pte);
}

int agx_uat_unmap_page(struct agx_uat *uat, u32 ctx_id, u64 iova)
{
	struct agx_uat_node *l1;
	struct agx_uat_node *l2;
	struct agx_uat_node *l3;
	u64 *leaf;

	if (ctx_id >= AGX_UAT_CONTEXT_COUNT || iova >= AGX_UAT_VA_LIMIT)
		return AGX_EINVAL;

	l1 = agx_uat_l1(uat, ctx_id, agx_uat_ttbr_select(iova), false, 0);
	if (!l1)
		return AGX_ENODATA;
	l2 = agx_uat_child(uat, l1, agx_uat_l1_index(iova), 0, false);
	if (!l2)
		return AGX_ENODATA;
	l3 = agx_uat_child(uat, l2, agx_uat_l2_index(iova), 0, false);
	if (!l3)
		return AGX_ENODATA;

	leaf = (u64 *)l3->cpu + agx_uat_l3_index(iova);
	if (!(AGX_LOAD_ACQ(*leaf) & AGX_UAT_PTE_VALID))
		return AGX_ENODATA;

	AGX_STORE_REL(*leaf, 0);
	return 0;
}

void agx_uat_free_context(struct agx_uat *uat, u32 ctx_id)
{
	u32 select;

	if (ctx_id >= AGX_UAT_CONTEXT_COUNT)
		return;

	for (select = 0; select < 2; select++) {
		agx_uat_free_node(uat, uat->l1[ctx_id][select], 1);
		uat->l1[ctx_id][select] = NULL;
		agx_uat_l0_write(uat, ctx_id, select, 0, 0);
	}
}
