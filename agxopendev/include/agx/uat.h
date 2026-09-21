#ifndef AGX_UAT_H
#define AGX_UAT_H

#include "agx/types.h"

#define AGX_UAT_PAGE_BITS 14
#define AGX_UAT_PAGE_SIZE (1ULL << AGX_UAT_PAGE_BITS)
#define AGX_UAT_PAGE_MASK (AGX_UAT_PAGE_SIZE - 1ULL)

#define AGX_UAT_L1_SHIFT 36
#define AGX_UAT_L1_BITS 6
#define AGX_UAT_L1_COUNT (1u << AGX_UAT_L1_BITS)
#define AGX_UAT_L1_MASK (AGX_UAT_L1_COUNT - 1u)

#define AGX_UAT_L2_SHIFT 25
#define AGX_UAT_L2_BITS 11
#define AGX_UAT_L2_COUNT (1u << AGX_UAT_L2_BITS)
#define AGX_UAT_L2_MASK (AGX_UAT_L2_COUNT - 1u)

#define AGX_UAT_L3_SHIFT 14
#define AGX_UAT_L3_BITS 11
#define AGX_UAT_L3_COUNT (1u << AGX_UAT_L3_BITS)
#define AGX_UAT_L3_MASK (AGX_UAT_L3_COUNT - 1u)

#define AGX_UAT_TTBR_SELECT_BIT AGX_BIT64(42)
#define AGX_UAT_VA_BITS 43
#define AGX_UAT_VA_LIMIT AGX_BIT64(AGX_UAT_VA_BITS)

#define AGX_UAT_CONTEXT_COUNT 64u
#define AGX_UAT_FW_CONTEXT 0u

#define AGX_UAT_PTE_VALID AGX_BIT64(0)
#define AGX_UAT_PTE_TYPE AGX_BIT64(1)
#define AGX_UAT_PTE_AP_SHIFT 6
#define AGX_UAT_PTE_AP_MASK 0x3ULL
#define AGX_UAT_PTE_SH_SHIFT 8
#define AGX_UAT_PTE_SH_MASK 0x3ULL
#define AGX_UAT_PTE_AF AGX_BIT64(10)
#define AGX_UAT_PTE_NG AGX_BIT64(11)
#define AGX_UAT_PTE_ATTRIDX_SHIFT 2
#define AGX_UAT_PTE_ATTRIDX_MASK 0x7ULL
#define AGX_UAT_PTE_OFFSET_SHIFT 14
#define AGX_UAT_PTE_OFFSET_MASK 0x3ffffffffULL
#define AGX_UAT_PTE_ADDR_MASK \
	(AGX_UAT_PTE_OFFSET_MASK << AGX_UAT_PTE_OFFSET_SHIFT)
#define AGX_UAT_PTE_PXN AGX_BIT64(53)
#define AGX_UAT_PTE_UXN AGX_BIT64(54)
#define AGX_UAT_PTE_OS AGX_BIT64(55)

#define AGX_UAT_ATTR_NORMAL 0u
#define AGX_UAT_ATTR_DEVICE 1u
#define AGX_UAT_ATTR_SHARED 2u

#define AGX_UAT_TTBR_VALID AGX_BIT64(0)
#define AGX_UAT_TTBR_BADDR_SHIFT 1
#define AGX_UAT_TTBR_BADDR_MASK 0x7fffffffffffULL
#define AGX_UAT_TTBR_ASID_SHIFT 48
#define AGX_UAT_TTBR_ASID_MASK 0xffffULL

static inline u32 agx_uat_l1_index(u64 va)
{
	return (u32)((va >> AGX_UAT_L1_SHIFT) & AGX_UAT_L1_MASK);
}

static inline u32 agx_uat_l2_index(u64 va)
{
	return (u32)((va >> AGX_UAT_L2_SHIFT) & AGX_UAT_L2_MASK);
}

static inline u32 agx_uat_l3_index(u64 va)
{
	return (u32)((va >> AGX_UAT_L3_SHIFT) & AGX_UAT_L3_MASK);
}

static inline u32 agx_uat_ttbr_select(u64 va)
{
	return (va & AGX_UAT_TTBR_SELECT_BIT) ? 1u : 0u;
}

static inline u64 agx_uat_page_offset(u64 va)
{
	return va & AGX_UAT_PAGE_MASK;
}

static inline u64 agx_uat_pte_phys(u64 pte)
{
	return pte & AGX_UAT_PTE_ADDR_MASK;
}

static inline u64 agx_uat_table_pte(u64 table_pa)
{
	return (table_pa & AGX_UAT_PTE_ADDR_MASK) | AGX_UAT_PTE_VALID |
	       AGX_UAT_PTE_TYPE;
}

static inline u64 agx_uat_table_pte_addr(u64 pte)
{
	return agx_uat_pte_phys(pte);
}

#define AGX_UAT_ATTR(n) \
	AGX_FIELD_PUT((n), AGX_UAT_PTE_ATTRIDX_SHIFT, AGX_UAT_PTE_ATTRIDX_MASK)
#define AGX_UAT_AP(n) AGX_FIELD_PUT((n), AGX_UAT_PTE_AP_SHIFT, AGX_UAT_PTE_AP_MASK)

#define AGX_UAT_FW_BUFFER_FLAGS                                          \
	(AGX_UAT_PTE_VALID | AGX_UAT_PTE_TYPE | AGX_UAT_PTE_OS |         \
	 AGX_UAT_PTE_UXN | AGX_UAT_PTE_AF | AGX_UAT_AP(1) |              \
	 AGX_UAT_ATTR(AGX_UAT_ATTR_NORMAL))

static inline u64 agx_uat_pte(u64 phys_addr, u64 flags)
{
	return (phys_addr & AGX_UAT_PTE_ADDR_MASK) |
	       (flags & ~AGX_UAT_PTE_ADDR_MASK);
}

static inline u64 agx_uat_ttbr(u64 table_pa, u32 asid)
{
	u64 baddr = (table_pa >> AGX_UAT_TTBR_BADDR_SHIFT) &
		    AGX_UAT_TTBR_BADDR_MASK;

	return (baddr << AGX_UAT_TTBR_BADDR_SHIFT) |
	       AGX_FIELD_PUT(asid, AGX_UAT_TTBR_ASID_SHIFT,
			     AGX_UAT_TTBR_ASID_MASK) |
	       (table_pa != 0 ? AGX_UAT_TTBR_VALID : 0);
}

#define AGX_HANDOFF_MAGIC 0x4b1d000000000002ULL

#define AGX_HANDOFF_OFF_MAGIC_AP 0x00u
#define AGX_HANDOFF_OFF_MAGIC_FW 0x08u
#define AGX_HANDOFF_OFF_LOCK_AP 0x10u
#define AGX_HANDOFF_OFF_LOCK_FW 0x11u
#define AGX_HANDOFF_OFF_TURN 0x14u
#define AGX_HANDOFF_OFF_CUR_CTX 0x18u
#define AGX_HANDOFF_OFF_FLUSH(ctx) (0x20u + 0x18u * (ctx))
#define AGX_HANDOFF_OFF_FLUSH_STATE(ctx) AGX_HANDOFF_OFF_FLUSH(ctx)
#define AGX_HANDOFF_OFF_FLUSH_ADDR(ctx) (AGX_HANDOFF_OFF_FLUSH(ctx) + 0x08u)
#define AGX_HANDOFF_OFF_FLUSH_SIZE(ctx) (AGX_HANDOFF_OFF_FLUSH(ctx) + 0x10u)
#define AGX_HANDOFF_FLUSH_SLOTS 0x41u
#define AGX_HANDOFF_KERNEL_FLUSH_CTX 0x40u
#define AGX_HANDOFF_SIZE 0x648u

#define AGX_HANDOFF_FLUSH_IDLE 0u
#define AGX_HANDOFF_FLUSH_BUSY 1u
#define AGX_HANDOFF_FLUSH_DONE 2u

#define AGX_HANDOFF_UNMAP_TAG 0xdead000000000000ULL
#define AGX_HANDOFF_UNMAP_ADDR_MASK 0xffffffffffffULL

#endif
