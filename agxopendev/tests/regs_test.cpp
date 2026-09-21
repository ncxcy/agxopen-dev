#include <cstdint>
#include <cstring>

#include "agx/gpu_ep.h"
#include "agx/mailbox_regs.h"
#include "agx/uat.h"
#include "agx/power.h"
#include "expect.hpp"

using agxtest::expect;

static void test_mailbox_bits()
{
	expect(agx_mbox_control_full(0x10000) == true, "full bit alone reads as full");
	expect(agx_mbox_control_full(0x00000) == false, "zero control does not read as full");
	expect(agx_mbox_control_full(0x20000) == false, "empty bit alone does not read as full");
	expect(agx_mbox_control_empty(0x20000) == true, "empty bit alone reads as empty");
	expect(agx_mbox_control_empty(0x10000) == false, "full bit alone does not read as empty");
	expect(agx_mbox_control_empty(0x30000) == true, "empty bit is found among other bits");
}

static void test_mailbox_offsets()
{
	expect(AGX_MBOX_A2I_CONTROL == 0x8000u + 0x110u, "a2i control sits at the block base plus 0x110");
	expect(AGX_MBOX_I2A_CONTROL == 0x8000u + 0x114u, "i2a control sits at the block base plus 0x114");
	expect(AGX_MBOX_A2I_SEND0 == 0x8000u + 0x800u, "a2i send0 sits at the block base plus 0x800");
	expect(AGX_MBOX_A2I_SEND1 == 0x8000u + 0x808u, "a2i send1 sits at the block base plus 0x808");
	expect(AGX_MBOX_I2A_RECV0 == 0x8000u + 0x830u, "i2a recv0 sits at the block base plus 0x830");
	expect(AGX_MBOX_I2A_RECV1 == 0x8000u + 0x838u, "i2a recv1 sits at the block base plus 0x838");
	expect(AGX_MBOX_A2I_CONTROL + 4 <= AGX_MBOX_REGION_SIZE &&
		       AGX_MBOX_I2A_CONTROL + 4 <= AGX_MBOX_REGION_SIZE &&
		       AGX_MBOX_A2I_SEND1 + 8 <= AGX_MBOX_REGION_SIZE &&
		       AGX_MBOX_I2A_RECV1 + 8 <= AGX_MBOX_REGION_SIZE,
	       "every register fits inside the required region size");
	expect(AGX_MBOX_REGION_SIZE == AGX_MBOX_I2A_RECV1 + 8, "region size ends right after the last register");
}

static void test_outbox_enable_bit()
{
	expect(AGX_MBOX_CONTROL_ENABLE == 1u, "enable is bit 0");
	expect(agx_mbox_control_with_enable(0x20000, true) == 0x20001, "enable sets bit 0 and keeps the empty bit");
	expect(agx_mbox_control_with_enable(0x20001, false) == 0x20000, "disable clears only bit 0");
	expect(agx_mbox_control_with_enable(0xfffffffeu, true) == 0xffffffffu, "enable leaves every other bit alone");
	expect(agx_mbox_control_with_enable(0xffffffffu, false) == 0xfffffffeu, "disable leaves every other bit alone");
}

static void test_mailbox_messages()
{
	struct agx_mbox_message msg = agx_mbox_unpack(0x1122334455667788ULL, 0xabcdef0000000021ULL);

	expect(msg.data == 0x1122334455667788ULL, "unpack keeps the data word");
	expect(msg.endpoint == 0x21, "unpack takes the endpoint from the low byte");
	expect(msg.flags == 0xabcdef00u, "unpack keeps the upper half as flags");
	expect(agx_mbox_pack_msg1(0x20) == 0x20, "pack keeps a valid endpoint");
	expect(agx_mbox_pack_msg1(0x1ff) == 0xff, "pack masks the endpoint to one byte");
	msg = agx_mbox_unpack(0, agx_mbox_pack_msg1(0x21));
	expect(msg.endpoint == 0x21 && msg.flags == 0, "pack then unpack round trips");
}

static void test_power()
{
	enum agx_power_state state = AGX_POWER_OFF;

	expect(agx_power_state_from_wire(AGX_PWR_ON, &state) && state == AGX_POWER_ON, "0x20 maps to on");
	expect(agx_power_state_from_wire(AGX_PWR_INIT, &state) && state == AGX_POWER_ON, "0x220 maps to on by its low byte");
	expect(agx_power_state_from_wire(AGX_PWR_IDLE, &state) && state == AGX_POWER_STANDBY, "0x201 maps to standby");
	expect(agx_power_state_from_wire(AGX_PWR_SLEEP, &state) && state == AGX_POWER_STANDBY, "0x01 maps to standby");
	expect(agx_power_state_from_wire(AGX_PWR_QUIESCED, &state) && state == AGX_POWER_QUIESCENT, "0x10 maps to quiescent");
	expect(agx_power_state_from_wire(AGX_PWR_OFF, &state) && state == AGX_POWER_OFF, "0x00 maps to off");
	expect(!agx_power_state_from_wire(AGX_PWR_UNKNOWN_202, &state), "0x202 is reported as unmapped");
	expect(!agx_power_state_from_wire(0xdead, &state), "random wire values are reported as unmapped");

	expect(std::strcmp(agx_power_state_name(AGX_POWER_OFF), "off") == 0, "off name");
	expect(std::strcmp(agx_power_state_name(AGX_POWER_QUIESCENT), "quiescent") == 0, "quiescent name");
	expect(std::strcmp(agx_power_state_name(AGX_POWER_STANDBY), "standby") == 0, "standby name");
	expect(std::strcmp(agx_power_state_name(AGX_POWER_ON), "on") == 0, "on name");
	expect(std::strcmp(agx_power_state_name(static_cast<enum agx_power_state>(99)), "unknown") == 0, "out of range name");
}

static void test_gpu_messages()
{
	expect(agx_gpu_msg_init(0x123) == 0x0081000000000123ULL, "init message layout");
	expect(agx_gpu_msg_init(0xffffffffffffffffULL) == 0x00810fffffffffffULL,
	       "init message masks the address to 44 bits");
	expect(agx_gpu_msg_type(agx_gpu_msg_init(1)) == AGX_GPU_MSG_INIT, "init message type decodes");
	expect(agx_gpu_msg_initdata(agx_gpu_msg_init(0x123456789000ULL)) == 0x23456789000ULL,
	       "init address round trips within 44 bits");
	expect(agx_gpu_msg_doorbell(0x10) == 0x0083000000000010ULL, "doorbell message layout");
	expect(agx_gpu_msg_doorbell(0x1ffff) == 0x008300000000ffffULL, "doorbell channel is masked to 16 bits");
	expect(agx_gpu_msg_channel(agx_gpu_msg_doorbell(7)) == 7, "doorbell channel round trips");
	expect(agx_gpu_msg_fwctl() == 0x0084000000000000ULL, "firmware control message layout");
	expect(agx_gpu_msg_type(0x0042000000000000ULL) == AGX_GPU_MSG_EVENT, "event message type decodes");
	expect(agx_gpu_msg_kick(4, false) == 0x0083000000000010ULL, "kick index 4 equals the m1n1 kick value 0x10");
	expect(agx_gpu_msg_kick(7, false) == 0x008300000000001cULL, "kick index 7 shifts left by two");
	expect(agx_gpu_msg_kick(0xf, false) == 0x008300000000001cULL, "kick index is masked to three bits");
	expect(agx_gpu_msg_kick(2, true) == 0x0086000000000008ULL, "alternate kick uses type 0x86");
	expect(agx_gpu_msg_stop(0x12345) == 0x0085000000012345ULL, "stop message carries a counter");
	expect(agx_gpu_msg_stop(0xffffffffffffffffULL) == 0x00850fffffffffffULL, "stop counter is masked to 44 bits");
	expect(agx_gpu_msg_is_event(0x0042000000000000ULL), "type 0x42 is an event");
	expect(agx_gpu_msg_is_event(0x0002000000000000ULL), "type 0x02 is an event because only six bits are compared");
	expect(agx_gpu_msg_is_event(0x00c2000000000123ULL), "type 0xc2 is an event because only six bits are compared");
	expect(!agx_gpu_msg_is_event(0x0081000000000000ULL), "type 0x81 is not an event");
	expect(!agx_gpu_msg_is_event(0x0083000000000000ULL), "type 0x83 is not an event");
}

static void test_uat_pte_helpers()
{
	expect(agx_uat_page_offset(0x123ULL) == 0x123ULL, "page offset extracts the low 14 bits");
	expect(agx_uat_page_offset(0x4000ULL) == 0, "an aligned address has a zero page offset");

	expect(AGX_UAT_FW_BUFFER_FLAGS == 0x00c0000000000443ULL,
	       "firmware buffer flags equal m1n1 iomap defaults: OS, UXN, AF, AP 1, TYPE, VALID, attr normal");
	u64 pte = agx_uat_pte(0x200004000ULL, AGX_UAT_FW_BUFFER_FLAGS);
	expect((pte & AGX_UAT_PTE_VALID) && (pte & AGX_UAT_PTE_TYPE) && (pte & AGX_UAT_PTE_OS) &&
		       (pte & AGX_UAT_PTE_AF) && (pte & AGX_UAT_PTE_UXN) && !(pte & AGX_UAT_PTE_PXN),
	       "firmware buffer pte sets os, uxn, af, type and valid but not pxn");
	expect(agx_uat_pte_phys(pte) == 0x200004000ULL,
	       "pte physical field round trips an aligned address");
	expect(pte == (0x200004000ULL | 0x00c0000000000443ULL),
	       "a firmware buffer pte is exactly the address ored with the flag set");
	u64 shared = agx_uat_pte(0x200004000ULL, AGX_UAT_FW_BUFFER_FLAGS | AGX_UAT_ATTR(AGX_UAT_ATTR_SHARED));
	expect(((shared >> AGX_UAT_PTE_ATTRIDX_SHIFT) & AGX_UAT_PTE_ATTRIDX_MASK) == AGX_UAT_ATTR_SHARED,
	       "the attribute helper places the memory type in bits 4 to 2");
	expect(agx_uat_pte_phys(agx_uat_pte(0xffffffffffffc000ULL, AGX_UAT_FW_BUFFER_FLAGS)) ==
		       0xffffffffc000ULL,
	       "the physical field is limited to bits 47 to 14 and drops stray high bits");
	expect(agx_uat_pte_phys(agx_uat_pte(0x123456780000ULL, AGX_UAT_FW_BUFFER_FLAGS)) ==
		       0x123456780000ULL,
	       "a physical address above 32 bits survives encoding without truncation");
	expect((agx_uat_pte(0x1000ULL, 0x00c0000000004443ULL) & 0x4000ULL) == 0,
	       "flag bits that overlap the address field never leak into the address");

	u64 ttbr = agx_uat_ttbr(0x300008000ULL, 7);
	expect((ttbr & AGX_UAT_TTBR_VALID) != 0, "a nonzero table address produces a valid ttbr");
	expect(((ttbr >> AGX_UAT_TTBR_ASID_SHIFT) & AGX_UAT_TTBR_ASID_MASK) == 7,
	       "ttbr carries the requested asid");
	expect((ttbr & 0xffffffffffffULL & ~1ULL) == 0x300008000ULL,
	       "ttbr keeps the table address in bits 47 to 1 as m1n1 encodes it");
	expect(agx_uat_table_pte_addr(agx_uat_table_pte(0x400010000ULL)) == 0x400010000ULL,
	       "a table pte round trips its physical address");
	expect(agx_uat_ttbr(0, 0) == 0, "a zero table address produces an invalid, all zero ttbr");
}

void run_uat_tests();
void run_handoff_tests();

int main()
{
	run_uat_tests();
	run_handoff_tests();
	test_uat_pte_helpers();
	test_mailbox_bits();
	test_mailbox_offsets();
	test_outbox_enable_bit();
	test_mailbox_messages();
	test_power();
	test_gpu_messages();
	return agxtest::finish();
}
