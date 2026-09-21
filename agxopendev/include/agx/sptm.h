#ifndef AGX_SPTM_H
#define AGX_SPTM_H

#include "agx/types.h"
#include "agx/sptmregisters.h"

#define SPTM_DOMAIN_SHIFT 48
#define SPTM_TABLEID_SHIFT 32
#define SPTM_TABLEID_MASK 0xfu

#define SPTM_CALL_DESC(domain, table_id) \
	(((u64)(domain) << SPTM_DOMAIN_SHIFT) | \
	 ((u64)((table_id) & SPTM_TABLEID_MASK) << SPTM_TABLEID_SHIFT))

enum sptm_domain {
	SPTM_DOMAIN_SPTM = 0,
	SPTM_DOMAIN_BOOTKCX = 1,
	SPTM_DOMAIN_TXM = 2
};

enum sptm_self_table {
	SPTM_TABLE_SELF_0 = 0,
	SPTM_TABLE_SELF_1 = 1,
	SPTM_TABLE_SELF_2 = 2,
	SPTM_TABLE_SELF_10 = 10
};

#endif
