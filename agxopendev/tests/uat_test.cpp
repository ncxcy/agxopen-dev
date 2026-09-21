#include <atomic>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

extern "C" {
#include "uat.h"
}

#include "expect.hpp"

using agxtest::expect;

namespace {

struct Arena {
	std::mutex m;
	u64 next_phys = 0x1000;
	int live_tables = 0;
	int live_bookkeeping = 0;
	std::set<u64> phys_in_use;

	static int alloc_table(void *ctx, void **cpu, u64 *phys)
	{
		Arena *a = static_cast<Arena *>(ctx);
		std::lock_guard<std::mutex> g(a->m);

		void *p = std::calloc(1, AGX_UAT_PAGE_SIZE);
		if (!p)
			return AGX_ENOMEM;
		*cpu = p;
		*phys = a->next_phys;
		a->phys_in_use.insert(a->next_phys);
		a->next_phys += AGX_UAT_PAGE_SIZE;
		a->live_tables++;
		return 0;
	}

	static void free_table(void *ctx, void *cpu, u64 phys)
	{
		Arena *a = static_cast<Arena *>(ctx);
		std::lock_guard<std::mutex> g(a->m);

		a->phys_in_use.erase(phys);
		a->live_tables--;
		std::free(cpu);
	}

	static void *alloc_bookkeeping(void *ctx, size_t size)
	{
		Arena *a = static_cast<Arena *>(ctx);
		std::lock_guard<std::mutex> g(a->m);

		void *p = std::calloc(1, size);
		if (p)
			a->live_bookkeeping++;
		return p;
	}

	static void free_bookkeeping(void *ctx, void *ptr, size_t size)
	{
		Arena *a = static_cast<Arena *>(ctx);
		(void)size;
		std::lock_guard<std::mutex> g(a->m);

		a->live_bookkeeping--;
		std::free(ptr);
	}
};

const struct agx_uat_ops kOps = {
	Arena::alloc_table,
	Arena::free_table,
	Arena::alloc_bookkeeping,
	Arena::free_bookkeeping,
};

long g_leaks = 0;

struct agx_uat_node *child_of(struct agx_uat_node *node, u32 idx)
{
	return static_cast<struct agx_uat_node *>(node->children[idx]);
}

struct Fixture {
	Arena arena;
	std::vector<u64> l0;
	struct agx_uat uat;

	Fixture() : l0(AGX_UAT_CONTEXT_COUNT * 2, 0)
	{
		agx_uat_init(&uat, &kOps, &arena, l0.data());
	}

	~Fixture()
	{
		for (u32 i = 0; i < AGX_UAT_CONTEXT_COUNT; i++)
			agx_uat_free_context(&uat, i);
		g_leaks += arena.live_tables + arena.live_bookkeeping;
	}
};

void test_geometry_matches_kernelcache()
{
	expect(AGX_UAT_PAGE_SIZE == 16384, "page size is 16 kilobytes");
	expect(AGX_UAT_L1_BITS == 6 && AGX_UAT_L1_SHIFT == 36,
	       "l1 index is bits 36 to 41, matching updatePageTableEntry pc_index");
	expect(AGX_UAT_L2_BITS == 11 && AGX_UAT_L2_SHIFT == 25,
	       "l2 index is bits 25 to 35, matching updatePageTableEntry pd_index");
	expect(AGX_UAT_L3_BITS == 11 && AGX_UAT_L3_SHIFT == 14,
	       "l3 index is bits 14 to 24, matching updatePageTableEntry pt_index");
	expect(agx_uat_l1_index(0x1234567890ULL) ==
		       ((0x1234567890ULL >> 36) & 0x3f),
	       "l1 index helper matches the raw shift and mask");
	expect(agx_uat_l2_index(0x1234567890ULL) ==
		       ((0x1234567890ULL >> 25) & 0x7ff),
	       "l2 index helper matches the raw shift and mask");
	expect(agx_uat_l3_index(0x1234567890ULL) ==
		       ((0x1234567890ULL >> 14) & 0x7ff),
	       "l3 index helper matches the raw shift and mask");
	expect(agx_uat_ttbr_select(AGX_UAT_TTBR_SELECT_BIT) == 1 &&
		       agx_uat_ttbr_select(0) == 0,
	       "ttbr select reads bit 42, matching the fGartTables[0]/[1] split");
	expect(AGX_HANDOFF_MAGIC == 0x4b1d000000000002ULL,
	       "handoff magic matches the literal written in initHandoff");
}

void test_single_page_round_trip()
{
	Fixture f;
	bool valid;

	u64 iova = 0x40000000ULL;
	u64 phys = 0x300000000ULL;

	expect(agx_uat_map_page(&f.uat, 3, iova, phys,
				AGX_UAT_FW_BUFFER_FLAGS |
					AGX_UAT_ATTR(AGX_UAT_ATTR_SHARED)) == 0,
	       "map a single page succeeds");
	u64 got = agx_uat_lookup(&f.uat, 3, iova, &valid);
	expect(valid && got == phys, "lookup returns the mapped physical page");

	struct agx_uat_node *n1 = f.uat.l1[3][0];
	struct agx_uat_node *n2 = child_of(n1, agx_uat_l1_index(iova));
	struct agx_uat_node *n3 = child_of(n2, agx_uat_l2_index(iova));
	u64 *l3 = static_cast<u64 *>(n3->cpu);
	u64 pte = l3[agx_uat_l3_index(iova)];
	expect((pte & AGX_UAT_PTE_VALID) && (pte & AGX_UAT_PTE_TYPE),
	       "leaf pte has valid and type bits set");
	expect(agx_uat_pte_phys(pte) == phys,
	       "leaf pte physical field matches the mapped address");
	expect(((pte >> AGX_UAT_PTE_ATTRIDX_SHIFT) & AGX_UAT_PTE_ATTRIDX_MASK) ==
		       AGX_UAT_ATTR_SHARED,
	       "leaf pte attr index matches the requested memory type");

	u64 l1_pte = f.l0[3 * 2 + 0];
	expect((l1_pte & AGX_UAT_TTBR_VALID) != 0,
	       "ttbr slot for context 3 select 0 is marked valid after mapping");
	expect((f.l0[3 * 2 + 1] & AGX_UAT_TTBR_VALID) == 0,
	       "the unused ttbr select stays invalid");

	expect(agx_uat_unmap_page(&f.uat, 3, iova) == 0, "unmap succeeds");
	got = agx_uat_lookup(&f.uat, 3, iova, &valid);
	expect(!valid, "lookup reports invalid after unmap");
	expect(agx_uat_unmap_page(&f.uat, 3, iova) == AGX_ENODATA,
	       "unmapping an already unmapped page fails with ENODATA");
}

void test_va_limit_covers_both_ttbr_halves()
{
	expect(AGX_UAT_VA_LIMIT == (1ULL << 43),
	       "the address space is 43 bits, bit 42 selecting between the two ttbr halves");
	expect(AGX_UAT_TTBR_SELECT_BIT < AGX_UAT_VA_LIMIT,
	       "the ttbr select bit lies inside the valid address space");
	Fixture f;
	bool valid;
	u64 top = AGX_UAT_VA_LIMIT - AGX_UAT_PAGE_SIZE;
	expect(agx_uat_map_page(&f.uat, 0, top, 0x700000000ULL, AGX_UAT_FW_BUFFER_FLAGS) == 0,
	       "the last page of the address space can be mapped");
	u64 got = agx_uat_lookup(&f.uat, 0, top, &valid);
	expect(valid && got == 0x700000000ULL, "the last page of the address space reads back");
}

void test_ttbr_select_bit_routes_to_separate_table()
{
	Fixture f;
	bool valid;

	u64 low = 0x1000000ULL;
	u64 high = low | AGX_UAT_TTBR_SELECT_BIT;

	agx_uat_map_page(&f.uat, 5, low, 0x500000000ULL, AGX_UAT_FW_BUFFER_FLAGS);
	agx_uat_map_page(&f.uat, 5, high, 0x600000000ULL, AGX_UAT_FW_BUFFER_FLAGS);

	expect(f.uat.l1[5][0] != f.uat.l1[5][1],
	       "the ttbr select bit routes to two independent l1 tables");
	u64 a = agx_uat_lookup(&f.uat, 5, low, &valid);
	expect(valid && a == 0x500000000ULL, "low half address maps correctly");
	u64 b = agx_uat_lookup(&f.uat, 5, high, &valid);
	expect(valid && b == 0x600000000ULL,
	       "high half address maps independently of the low half");
}

void test_contexts_are_independent()
{
	Fixture f;
	bool valid;

	agx_uat_map_page(&f.uat, 1, 0x10000000ULL, 0x800000000ULL, AGX_UAT_FW_BUFFER_FLAGS);
	agx_uat_map_page(&f.uat, 2, 0x10000000ULL, 0x900000000ULL, AGX_UAT_FW_BUFFER_FLAGS);

	u64 a = agx_uat_lookup(&f.uat, 1, 0x10000000ULL, &valid);
	expect(valid && a == 0x800000000ULL, "context 1 keeps its own mapping");
	u64 b = agx_uat_lookup(&f.uat, 2, 0x10000000ULL, &valid);
	expect(valid && b == 0x900000000ULL,
	       "context 2 maps the same iova to a different address");
	expect(f.uat.l1[1][0] != f.uat.l1[2][0],
	       "the two contexts use separate l1 tables");
}

void test_crosses_l3_and_l2_table_boundaries()
{
	Fixture f;
	bool valid;
	int bad = 0;
	const u64 l3_span = static_cast<u64>(AGX_UAT_L3_COUNT) << AGX_UAT_L3_SHIFT;
	const u64 base = 0x20000000ULL;

	for (int i = -2; i <= 2; i++) {
		u64 iova = base + static_cast<u64>(i) * l3_span;
		u64 phys = 0xa00000000ULL + static_cast<u64>(i + 2) * AGX_UAT_PAGE_SIZE;
		if (agx_uat_map_page(&f.uat, 7, iova, phys, AGX_UAT_FW_BUFFER_FLAGS) != 0)
			bad++;
	}
	expect(bad == 0, "mapping pages spaced across l3 table boundaries succeeds");

	for (int i = -2; i <= 2; i++) {
		u64 iova = base + static_cast<u64>(i) * l3_span;
		u64 expect_phys = 0xa00000000ULL + static_cast<u64>(i + 2) * AGX_UAT_PAGE_SIZE;
		u64 got = agx_uat_lookup(&f.uat, 7, iova, &valid);
		if (!valid || got != expect_phys)
			bad++;
	}
	expect(bad == 0, "every page spaced across l3 boundaries reads back correctly");

	const u64 l2_span = static_cast<u64>(AGX_UAT_L2_COUNT) * l3_span;
	u64 far_iova = base + l2_span;
	expect(agx_uat_map_page(&f.uat, 7, far_iova, 0xb00000000ULL, AGX_UAT_FW_BUFFER_FLAGS) == 0,
	       "mapping a page one l2 table away from the first also succeeds");
	u64 got = agx_uat_lookup(&f.uat, 7, far_iova, &valid);
	expect(valid && got == 0xb00000000ULL,
	       "the page one l2 table away reads back correctly");
	u64 near = agx_uat_lookup(&f.uat, 7, base, &valid);
	expect(valid && near == 0xa00000000ULL + 2 * AGX_UAT_PAGE_SIZE,
	       "mapping across an l2 boundary does not disturb the original page");
}

void test_double_map_overwrites()
{
	Fixture f;
	bool valid;
	u64 iova = 0x70000000ULL;

	agx_uat_map_page(&f.uat, 9, iova, 0xc00000000ULL, AGX_UAT_FW_BUFFER_FLAGS);
	agx_uat_map_page(&f.uat, 9, iova, 0xd00000000ULL, AGX_UAT_FW_BUFFER_FLAGS);
	u64 got = agx_uat_lookup(&f.uat, 9, iova, &valid);
	expect(valid && got == 0xd00000000ULL,
	       "mapping the same iova twice overwrites the previous translation");
}

void test_rejects_bad_input()
{
	Fixture f;

	expect(agx_uat_map_page(&f.uat, AGX_UAT_CONTEXT_COUNT, 0, 0, AGX_UAT_FW_BUFFER_FLAGS) ==
		       AGX_EINVAL,
	       "context id past the limit is rejected");
	expect(agx_uat_map_page(&f.uat, 0, 0x1000, 0, AGX_UAT_FW_BUFFER_FLAGS) == AGX_EINVAL,
	       "unaligned iova is rejected");
	expect(agx_uat_map_page(&f.uat, 0, 0, 0x1000, AGX_UAT_FW_BUFFER_FLAGS) == AGX_EINVAL,
	       "unaligned physical address is rejected");
	expect(agx_uat_map_page(&f.uat, 0, AGX_UAT_VA_LIMIT, 0, AGX_UAT_FW_BUFFER_FLAGS) ==
		       AGX_EINVAL,
	       "iova at or past the 43 bit limit is rejected");
	bool valid;
	agx_uat_lookup(&f.uat, AGX_UAT_CONTEXT_COUNT, 0, &valid);
	expect(!valid, "lookup on an out of range context reports invalid rather than crashing");
	agx_uat_lookup(&f.uat, 0, 0x123456, &valid);
	expect(!valid, "lookup on a never mapped table walks safely and reports invalid");
}

void test_free_context_releases_everything()
{
	Fixture f;

	for (int i = 0; i < 50; i++)
		agx_uat_map_page(&f.uat, 11, static_cast<u64>(i) * AGX_UAT_PAGE_SIZE,
				 0xe00000000ULL + static_cast<u64>(i) * AGX_UAT_PAGE_SIZE, AGX_UAT_FW_BUFFER_FLAGS);
	agx_uat_map_page(&f.uat, 11,
			 (static_cast<u64>(AGX_UAT_L3_COUNT) << AGX_UAT_L3_SHIFT) +
				 AGX_UAT_PAGE_SIZE,
			 0xf00000000ULL, AGX_UAT_FW_BUFFER_FLAGS);

	expect(f.arena.live_tables > 0, "mapping allocated hardware table pages");
	agx_uat_free_context(&f.uat, 11);
	expect(f.arena.live_tables == 0, "freeing the context releases every table page");
	expect(f.arena.live_bookkeeping == 0,
	       "freeing the context releases every bookkeeping node");
	expect(f.l0[11 * 2] == 0 && f.l0[11 * 2 + 1] == 0,
	       "freeing the context clears both ttbr slots");

	bool valid;
	agx_uat_lookup(&f.uat, 11, 0, &valid);
	expect(!valid, "a freed context reports every lookup as invalid");
	expect(agx_uat_map_page(&f.uat, 11, 0, 0x1230000ULL, AGX_UAT_FW_BUFFER_FLAGS) == 0,
	       "a freed context can be mapped again from scratch");
}

void test_fuzz_against_reference_map()
{
	Fixture f;
	std::map<u64, u64> reference;
	uint64_t seed = 0xda942042e4dd58b5ULL;
	auto next = [&]() {
		seed ^= seed << 13;
		seed ^= seed >> 7;
		seed ^= seed << 17;
		return seed;
	};

	for (int i = 0; i < 4000; i++) {
		u64 iova = (next() % (1ULL << 30)) & ~static_cast<u64>(AGX_UAT_PAGE_MASK);
		if (next() % 4 == 0 && !reference.empty()) {
			auto it = reference.begin();
			std::advance(it, next() % reference.size());
			agx_uat_unmap_page(&f.uat, 13, it->first);
			reference.erase(it);
			continue;
		}
		u64 phys = ((next() % (1ULL << 20)) << AGX_UAT_PAGE_BITS) + 0x40000000ULL;
		agx_uat_map_page(&f.uat, 13, iova, phys, AGX_UAT_FW_BUFFER_FLAGS);
		reference[iova] = phys;
	}

	int mismatches = 0;
	for (const auto &kv : reference) {
		bool valid;
		u64 got = agx_uat_lookup(&f.uat, 13, kv.first, &valid);
		if (!valid || got != kv.second)
			mismatches++;
	}
	expect(mismatches == 0,
	       "4000 random map and unmap operations match a reference map exactly");
}

}

void run_uat_tests()
{
	test_geometry_matches_kernelcache();
	test_single_page_round_trip();
	test_va_limit_covers_both_ttbr_halves();
	test_ttbr_select_bit_routes_to_separate_table();
	test_contexts_are_independent();
	test_crosses_l3_and_l2_table_boundaries();
	test_double_map_overwrites();
	test_rejects_bad_input();
	test_free_context_releases_everything();
	test_fuzz_against_reference_map();
	expect(g_leaks == 0,
	       "every table page and bookkeeping node from every test was released by free_context");
}
