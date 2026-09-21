#include "agx/types.h"
#include "agx/power.h"
#include "agx/gpu_ep.h"
#include "agx/mailbox_regs.h"
#include "agx/rtkit.h"
#include "agx/sptm.h"
#include "agx/sptmregisters.h"
#include "agx/sptmdispatch.h"
#include "agx/sptmbootregions.h"
#include "agx/sptmgpuiouat.h"
#include "agx/txm.h"

#include <stdio.h>

int main(void)
{
	u64 desc = SPTM_CALL_DESC(SPTM_DOMAIN_TXM, 5);
	struct agx_mbox_message msg = agx_mbox_unpack(1, agx_mbox_pack_msg1(0x21));
	int failures = 0;

	if (desc != ((2ULL << 48) | (5ULL << 32)))
		failures++;
	if (msg.endpoint != 0x21 || msg.data != 1)
		failures++;
	if (sptmUatRetypeInAddr != 0xfffffff0270957b8ULL)
		failures++;
	if (txmDispatchEntryAddr != 0xfffffff017026c68ULL)
		failures++;
	if (agx_gpu_msg_doorbell(0x10) != 0x0083000000000010ULL)
		failures++;
	if (sptmBootRegionTableCount != 35)
		failures++;
	if (TXM_CALL_MAX_SUPPORTED != 45)
		failures++;

	printf("portable c: %s\n", failures ? "fail" : "pass");
	return failures ? 1 : 0;
}
