#ifndef AGX_SPTMGPUIOUAT_H
#define AGX_SPTMGPUIOUAT_H

#include "agx/types.h"

#define sptmUatIsEnabledAddr 0xfffffff0270966e4ULL
#define sptmUatBootstrapParseDtAddr 0xfffffff027095850ULL
#define sptmUatRetypeInAddr 0xfffffff0270957b8ULL
#define sptmUatRetypeOutAddr 0xfffffff027095730ULL

enum sptmUatPageType {
	UAT_PAGE_TYPE_XNU_DEFAULT = 11,
	UAT_PAGE_TYPE_XNU_IOMMU   = 23,
};

enum sptmUatRetypeInSubtype {
	UAT_RETYPE_IN_SUB_0 = 0,
	UAT_RETYPE_IN_SUB_1 = 1,
	UAT_RETYPE_IN_SUB_4 = 4,
	UAT_RETYPE_IN_SUB_5 = 5,
};

enum sptmUatRetypeOutSubtype {
	UAT_RETYPE_OUT_SUB_0 = 0,
	UAT_RETYPE_OUT_SUB_3 = 3,
	UAT_RETYPE_OUT_SUB_4 = 4,
	UAT_RETYPE_OUT_SUB_5 = 5,
};

struct sptmUatDeviceTreeProps {
	u64 gfx_shared_region_base;
	u64 gfx_shared_region_size;
	u64 gfx_shared_l2_region_base;
	u64 gfx_shared_l2_region_size;
	u64 gpu_region_base;
	u64 gpu_region_size;
	u64 gfx_handoff_base;
	u64 gfx_handoff_size;
	u32 gpu_iouat_enabled;
	u32 uat_enforce_gpu_carveout;
	u8  uat_vaddr_size;
	u32 uat_segment_limit;
	u32 uat_mapping_limit;
};

#endif
