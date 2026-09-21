#ifndef AGX_TXM_H
#define AGX_TXM_H

#include "agx/types.h"

#define txmInitEntryAddr 0xfffffff01701ff04ULL
#define txmDispatchEntryAddr 0xfffffff017026c68ULL
#define txmPageEnforcementAddr 0xfffffff017026a64ULL
#define txmCsEnforcementAddr 0xfffffff01701c8a8ULL
#define txmHopValidatorAddr 0xfffffff017028768ULL
#define txmPanicAddr 0xfffffff01700586fULL

enum txmHopFrameTag {
	TXM_HOP_FRAME_TAG = 42,
};

enum txmPageEnforcementType {
	TXM_PAGE_EXEC_CS     = 0,
	TXM_PAGE_EXEC_NOMAP  = 1,
	TXM_PAGE_UNMAP       = 2,
	TXM_PAGE_EXEC_DEBUG  = 3,
	TXM_PAGE_WRITE_DEBUG = 4,
	TXM_PAGE_EXEC_SHARED = 5,
};

enum txmCsEnforcementType {
	TXM_CS_QUERY_EXEC     = 14,
	TXM_CS_QUERY_WRITE    = 15,
	TXM_CS_QUERY_SHARED   = 16,
};

enum txmCsEntitlement {
	TXM_CS_ENT_DEBUGGER        = 0,
	TXM_CS_ENT_DYNAMIC_CODESIG = 1,
	TXM_CS_ENT_PLATFORM_ONLY   = 2,
};

enum txmPageError {
	TXM_PAGE_OK              = 0,
	TXM_PAGE_ERR_DISALLOWED  = 28,
	TXM_PAGE_ERR_DEBUG_EXEC  = 30,
	TXM_PAGE_ERR_WRITE_CS    = 31,
	TXM_PAGE_ERR_WRITE_NOMAP = 32,
	TXM_PAGE_ERR_TRUST       = 34,
};

enum txmDispatchCallId {
	TXM_CALL_TRUST_CACHE_SETUP    = 1,
	TXM_CALL_TRUST_CACHE_INVAL    = 2,
	TXM_CALL_IMAGE4_DISPATCH      = 3,
	TXM_CALL_SECURE_TIME          = 4,
	TXM_CALL_LOCK_DOWN            = 5,
	TXM_CALL_TC_QUERY_0           = 6,
	TXM_CALL_TC_QUERY_1           = 7,
	TXM_CALL_TC_QUERY_2           = 8,
	TXM_CALL_CLEAR_0              = 9,
	TXM_CALL_CLEAR_1              = 10,
	TXM_CALL_TC_QUERY_3           = 11,
	TXM_CALL_PAGE_ENFORCE         = 12,
	TXM_CALL_IMAGE4_ADD           = 13,
	TXM_CALL_VERIFY_SIG           = 14,
	TXM_CALL_QUERY_CS             = 15,
	TXM_CALL_QUERY_PLATFORM_CS    = 16,
	TXM_CALL_EXEC_MAP             = 17,
	TXM_CALL_WRITE_MAP            = 18,
	TXM_CALL_QUERY_STATE          = 19,
	TXM_CALL_EXEC_MAP_EX          = 20,
	TXM_CALL_QUERY_STATE_EX       = 21,
	TXM_CALL_WRITE_MAP_EX         = 22,
	TXM_CALL_QUERY_TRUST          = 23,
	TXM_CALL_TC_LOOKUP            = 24,
	TXM_CALL_NOOP_25              = 25,
	TXM_CALL_ENTITLEMENT_CHECK    = 26,
	TXM_CALL_DEV_MODE_QUERY       = 27,
	TXM_CALL_ASSOC_SPAWN          = 28,
	TXM_CALL_ASSOC_EXEC           = 29,
	TXM_CALL_RESTRICT_EXEC        = 30,
	TXM_CALL_NOP                  = 31,
	TXM_CALL_QUERY_BOOT_VARIANT   = 32,
	TXM_CALL_TC_FETCH             = 33,
	TXM_CALL_QUERY_LOCK_STATE     = 34,
	TXM_CALL_MAP_PAIR             = 35,
	TXM_CALL_MAP_WITH_FLAGS       = 36,
	TXM_CALL_TC_FETCH_EX          = 37,
	TXM_CALL_EXEC_REGION          = 38,
	TXM_CALL_QUERY_EXEC_REGION    = 39,
	TXM_CALL_WRITE_REGION         = 40,
	TXM_CALL_QUERY_WRITE_REGION   = 41,
	TXM_CALL_EXEC_REGION_EX       = 42,
	TXM_CALL_QUERY_TRUST_EX       = 43,
	TXM_CALL_AMFI_QUERY           = 44,
	TXM_CALL_SECURE_CHANNEL       = 45,
	TXM_CALL_MAX_SUPPORTED        = 45,
};

enum txmDispatchError {
	TXM_DISP_OK               = 0,
	TXM_DISP_ERR_UNSUPPORTED  = 38,
};

#endif
