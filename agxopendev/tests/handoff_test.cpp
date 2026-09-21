#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>

extern "C" {
#include "gfxhandoff.h"
}

#include "expect.hpp"

using agxtest::expect;

namespace {

struct FakeHandoff {
	alignas(8) unsigned char bytes[AGX_HANDOFF_SIZE];

	FakeHandoff() { std::memset(bytes, 0, sizeof(bytes)); }

	static u8 r8(void *ctx, u32 off)
	{
		return __atomic_load_n(&static_cast<FakeHandoff *>(ctx)->bytes[off],
				       __ATOMIC_SEQ_CST);
	}
	static void w8(void *ctx, u32 off, u8 v)
	{
		__atomic_store_n(&static_cast<FakeHandoff *>(ctx)->bytes[off], v,
				 __ATOMIC_SEQ_CST);
	}
	static u32 r32(void *ctx, u32 off)
	{
		return __atomic_load_n(reinterpret_cast<u32 *>(
					       static_cast<FakeHandoff *>(ctx)->bytes + off),
				       __ATOMIC_SEQ_CST);
	}
	static void w32(void *ctx, u32 off, u32 v)
	{
		__atomic_store_n(reinterpret_cast<u32 *>(
					 static_cast<FakeHandoff *>(ctx)->bytes + off),
				 v, __ATOMIC_SEQ_CST);
	}
	static u64 r64(void *ctx, u32 off)
	{
		return __atomic_load_n(reinterpret_cast<u64 *>(
					       static_cast<FakeHandoff *>(ctx)->bytes + off),
				       __ATOMIC_SEQ_CST);
	}
	static void w64(void *ctx, u32 off, u64 v)
	{
		__atomic_store_n(reinterpret_cast<u64 *>(
					 static_cast<FakeHandoff *>(ctx)->bytes + off),
				 v, __ATOMIC_SEQ_CST);
	}
	static void relax(void *) { std::this_thread::yield(); }
	static void barrier(void *)
	{
		static std::atomic<int> sink(0);
		sink.fetch_add(0, std::memory_order_seq_cst);
	}
};

const struct agx_handoff_ops kOps = {
	FakeHandoff::r8,  FakeHandoff::w8,  FakeHandoff::r32,
	FakeHandoff::w32, FakeHandoff::r64, FakeHandoff::w64,
	FakeHandoff::relax, FakeHandoff::barrier,
};

void test_offsets_match_kernelcache()
{
	expect(AGX_HANDOFF_OFF_MAGIC_AP == 0x00, "magic ap offset");
	expect(AGX_HANDOFF_OFF_MAGIC_FW == 0x08, "magic fw offset");
	expect(AGX_HANDOFF_OFF_LOCK_AP == 0x10, "lock ap offset");
	expect(AGX_HANDOFF_OFF_LOCK_FW == 0x11, "lock fw offset");
	expect(AGX_HANDOFF_OFF_TURN == 0x14, "turn offset");
	expect(AGX_HANDOFF_OFF_CUR_CTX == 0x18, "current context offset");
	expect(AGX_HANDOFF_OFF_FLUSH_STATE(0) == 0x20, "flush slot 0 state offset");
	expect(AGX_HANDOFF_OFF_FLUSH_ADDR(0) == 0x28, "flush slot 0 address offset");
	expect(AGX_HANDOFF_OFF_FLUSH_SIZE(0) == 0x30, "flush slot 0 size offset");
	expect(AGX_HANDOFF_OFF_FLUSH_STATE(1) == 0x38, "flush slot 1 follows slot 0 by 0x18");
	expect(AGX_HANDOFF_OFF_FLUSH_STATE(0x40) == 0x20 + 0x40 * 0x18,
	       "kernel flush context slot 0x40 lands at the expected offset");
}

void test_wait_firmware_and_lock_unlock()
{
	FakeHandoff hw;
	struct agx_handoff ho;

	agx_handoff_init(&ho, &kOps, &hw);

	std::thread fw([&] {
		while (FakeHandoff::r64(&hw, AGX_HANDOFF_OFF_MAGIC_AP) !=
		       AGX_HANDOFF_MAGIC)
			std::this_thread::yield();
		FakeHandoff::w64(&hw, AGX_HANDOFF_OFF_MAGIC_FW, AGX_HANDOFF_MAGIC);
	});

	int ret = agx_handoff_wait_firmware(&ho, 2000000);
	fw.join();
	expect(ret == 0, "wait for firmware magic succeeds once firmware answers");
	expect(ho.initialized, "handoff is marked initialized after the magic exchange");
	expect(!ho.locked, "the lock is released again after the handshake");

	for (u32 i = 0; i < AGX_HANDOFF_FLUSH_SLOTS; i++) {
		expect(FakeHandoff::r64(&hw, AGX_HANDOFF_OFF_FLUSH_STATE(i)) == 0 &&
			       FakeHandoff::r64(&hw, AGX_HANDOFF_OFF_FLUSH_ADDR(i)) == 0 &&
			       FakeHandoff::r64(&hw, AGX_HANDOFF_OFF_FLUSH_SIZE(i)) == 0,
		       "every flush slot is cleared after initialization");
	}

	ret = agx_handoff_lock(&ho, 100000);
	expect(ret == 0 && ho.locked, "lock succeeds when firmware never holds the lock");
	agx_handoff_unlock(&ho);
	expect(!ho.locked && FakeHandoff::r8(&hw, AGX_HANDOFF_OFF_LOCK_AP) == 0,
	       "unlock clears the ap lock byte");
}

void test_lock_times_out_when_firmware_holds_it()
{
	FakeHandoff hw;
	struct agx_handoff ho;

	agx_handoff_init(&ho, &kOps, &hw);
	FakeHandoff::w8(&hw, AGX_HANDOFF_OFF_LOCK_FW, 1);

	int ret = agx_handoff_lock(&ho, 2000);
	expect(ret == AGX_ETIMEDOUT,
	       "lock times out when the firmware side never releases its lock");
	expect(FakeHandoff::r8(&hw, AGX_HANDOFF_OFF_LOCK_AP) == 0,
	       "a timed out lock attempt leaves the ap lock byte clear");
}

struct MirroredFirmwareLock {
	FakeHandoff *hw;

	void lock()
	{
		FakeHandoff::w8(hw, AGX_HANDOFF_OFF_LOCK_FW, 1);
		while (FakeHandoff::r8(hw, AGX_HANDOFF_OFF_LOCK_AP) != 0) {
			if (FakeHandoff::r32(hw, AGX_HANDOFF_OFF_TURN) == 0) {
				FakeHandoff::w8(hw, AGX_HANDOFF_OFF_LOCK_FW, 0);
				while (FakeHandoff::r32(hw, AGX_HANDOFF_OFF_TURN) == 0)
					std::this_thread::yield();
				FakeHandoff::w8(hw, AGX_HANDOFF_OFF_LOCK_FW, 1);
			}
			std::this_thread::yield();
		}
	}

	void unlock()
	{
		FakeHandoff::w32(hw, AGX_HANDOFF_OFF_TURN, 0);
		FakeHandoff::w8(hw, AGX_HANDOFF_OFF_LOCK_FW, 0);
	}
};

void test_dekker_mutual_exclusion_under_threads()
{
	FakeHandoff hw;
	struct agx_handoff ho_ap;
	MirroredFirmwareLock fw_lock{&hw};

	agx_handoff_init(&ho_ap, &kOps, &hw);
	FakeHandoff::w32(&hw, AGX_HANDOFF_OFF_TURN, 0);

	std::atomic<int> inside(0);
	std::atomic<bool> violation(false);
	std::atomic<long> ap_sections(0);
	std::atomic<long> fw_sections(0);
	const int iterations = 8000;

	std::thread ap([&] {
		for (int i = 0; i < iterations; i++) {
			if (agx_handoff_lock(&ho_ap, 20000000) != 0)
				continue;
			if (inside.fetch_add(1) != 0)
				violation.store(true);
			ap_sections.fetch_add(1);
			inside.fetch_sub(1);
			agx_handoff_unlock(&ho_ap);
		}
	});
	std::thread fw([&] {
		for (int i = 0; i < iterations; i++) {
			fw_lock.lock();
			if (inside.fetch_add(1) != 0)
				violation.store(true);
			fw_sections.fetch_add(1);
			inside.fetch_sub(1);
			fw_lock.unlock();
		}
	});

	ap.join();
	fw.join();

	expect(!violation.load(),
	       "the real ap lock implementation and a mirrored firmware implementation never enter the critical section together");
	expect(ap_sections.load() == iterations,
	       "the ap side completed every iteration without deadlocking");
	expect(fw_sections.load() == iterations,
	       "the mirrored firmware side completed every iteration without deadlocking");
}

void test_cacheflush_protocol()
{
	FakeHandoff hw;
	struct agx_handoff ho;

	agx_handoff_init(&ho, &kOps, &hw);

	expect(agx_handoff_prepare_cacheflush(&ho, AGX_HANDOFF_KERNEL_FLUSH_CTX,
					      0x1000, 0x4000) == 0,
	       "prepare cacheflush on the kernel context succeeds");
	expect(FakeHandoff::r64(&hw, AGX_HANDOFF_OFF_FLUSH_ADDR(
					     AGX_HANDOFF_KERNEL_FLUSH_CTX)) ==
		       0x1000,
	       "prepare writes the flush address");
	expect(FakeHandoff::r64(&hw, AGX_HANDOFF_OFF_FLUSH_STATE(
					     AGX_HANDOFF_KERNEL_FLUSH_CTX)) ==
		       AGX_HANDOFF_FLUSH_BUSY,
	       "prepare marks the slot busy");
	expect(agx_handoff_prepare_cacheflush(&ho, AGX_HANDOFF_KERNEL_FLUSH_CTX,
					      0x2000, 0x4000) == AGX_EINVAL,
	       "preparing a slot that is already busy is refused");

	std::thread fw([&] {
		while (FakeHandoff::r64(&hw, AGX_HANDOFF_OFF_FLUSH_STATE(
						     AGX_HANDOFF_KERNEL_FLUSH_CTX)) !=
		       AGX_HANDOFF_FLUSH_BUSY)
			std::this_thread::yield();
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
		FakeHandoff::w64(&hw,
				 AGX_HANDOFF_OFF_FLUSH_STATE(AGX_HANDOFF_KERNEL_FLUSH_CTX),
				 AGX_HANDOFF_FLUSH_DONE);
	});

	int ret = agx_handoff_wait_cacheflush(&ho, AGX_HANDOFF_KERNEL_FLUSH_CTX,
					      5000000);
	fw.join();
	expect(ret == 0, "waiting for the cacheflush completes once firmware finishes");
	expect(agx_handoff_complete_cacheflush(&ho, AGX_HANDOFF_KERNEL_FLUSH_CTX) ==
		       0,
	       "completing an idle-again cacheflush succeeds");
	expect(FakeHandoff::r64(&hw, AGX_HANDOFF_OFF_FLUSH_STATE(
					     AGX_HANDOFF_KERNEL_FLUSH_CTX)) == 0,
	       "the slot returns to idle after completion");
	expect(agx_handoff_complete_cacheflush(&ho, AGX_HANDOFF_KERNEL_FLUSH_CTX) ==
		       AGX_EINVAL,
	       "completing an already idle slot a second time is refused");
}

void test_cacheflush_timeout()
{
	FakeHandoff hw;
	struct agx_handoff ho;

	agx_handoff_init(&ho, &kOps, &hw);
	agx_handoff_prepare_cacheflush(&ho, 5, 0x9000, 0x1000);
	int ret = agx_handoff_wait_cacheflush(&ho, 5, 2000);
	expect(ret == AGX_ETIMEDOUT,
	       "waiting on a cacheflush that firmware never completes times out");
}

void test_unmap_protocol()
{
	FakeHandoff hw;
	struct agx_handoff ho;

	agx_handoff_init(&ho, &kOps, &hw);

	expect(agx_handoff_prepare_unmap(&ho, 2, 0x12345000, 0x2000) == 0,
	       "prepare unmap succeeds");
	u64 addr = FakeHandoff::r64(&hw, AGX_HANDOFF_OFF_FLUSH_ADDR(2));
	expect((addr & AGX_HANDOFF_UNMAP_TAG) == AGX_HANDOFF_UNMAP_TAG,
	       "prepare unmap tags the address with the dead marker");
	expect((addr & AGX_HANDOFF_UNMAP_ADDR_MASK) == 0x12345000,
	       "the tagged address preserves the original base in the low 48 bits");
	expect(FakeHandoff::r64(&hw, AGX_HANDOFF_OFF_FLUSH_STATE(2)) ==
		       AGX_HANDOFF_FLUSH_DONE,
	       "prepare unmap marks the slot done immediately, matching m1n1");
	expect(agx_handoff_complete_unmap(&ho, 2) == 0, "complete unmap succeeds");
	expect(FakeHandoff::r64(&hw, AGX_HANDOFF_OFF_FLUSH_STATE(2)) == 0,
	       "complete unmap returns the slot to idle");
}

}

void run_handoff_tests()
{
	test_offsets_match_kernelcache();
	test_wait_firmware_and_lock_unlock();
	test_lock_times_out_when_firmware_holds_it();
	test_dekker_mutual_exclusion_under_threads();
	test_cacheflush_protocol();
	test_cacheflush_timeout();
	test_unmap_protocol();
}
