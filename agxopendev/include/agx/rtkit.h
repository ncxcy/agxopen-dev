#ifndef AGX_RTKIT_H
#define AGX_RTKIT_H

#include "agx/types.h"
#include "agx/power.h"

#define AGX_RTKIT_EP_MGMT 0u
#define AGX_RTKIT_EP_CRASHLOG 1u
#define AGX_RTKIT_EP_SYSLOG 2u
#define AGX_RTKIT_EP_DEBUG 3u
#define AGX_RTKIT_EP_IOREPORT 4u
#define AGX_RTKIT_EP_OSLOG 8u
#define AGX_RTKIT_EP_APP_START 0x20u
#define AGX_RTKIT_EP_FIRMWARE 0x20u
#define AGX_RTKIT_EP_DOORBELL 0x21u
#define AGX_RTKIT_EP_COUNT 256u

#define AGX_RTKIT_MGMT_HELLO 1u
#define AGX_RTKIT_MGMT_HELLO_REPLY 2u
#define AGX_RTKIT_MGMT_STARTEP 5u
#define AGX_RTKIT_MGMT_IOP_PWR 6u
#define AGX_RTKIT_MGMT_IOP_PWR_ACK 7u
#define AGX_RTKIT_MGMT_EPMAP 8u
#define AGX_RTKIT_MGMT_EPMAP_REPLY 8u
#define AGX_RTKIT_MGMT_AP_PWR 0xbu
#define AGX_RTKIT_MGMT_AP_PWR_ACK 0xbu

#define AGX_RTKIT_TYPE_SHIFT 52
#define AGX_RTKIT_TYPE_MASK 0xffULL
#define AGX_RTKIT_OSLOG_TYPE_SHIFT 56

#define AGX_RTKIT_HELLO_MINVER_SHIFT 0
#define AGX_RTKIT_HELLO_MAXVER_SHIFT 16
#define AGX_RTKIT_HELLO_VER_MASK 0xffffULL

#define AGX_RTKIT_EPMAP_LAST AGX_BIT64(51)
#define AGX_RTKIT_EPMAP_MORE AGX_BIT64(0)
#define AGX_RTKIT_EPMAP_BASE_SHIFT 32
#define AGX_RTKIT_EPMAP_BASE_MASK 0x7ULL
#define AGX_RTKIT_EPMAP_BITMAP_MASK 0xffffffffULL

#define AGX_RTKIT_STARTEP_EP_SHIFT 32
#define AGX_RTKIT_STARTEP_EP_MASK 0xffULL
#define AGX_RTKIT_STARTEP_FLAG AGX_BIT64(1)

#define AGX_RTKIT_PWR_STATE_MASK 0xffffULL

#define AGX_RTKIT_PROTO_MIN 11u
#define AGX_RTKIT_PROTO_MAX 12u

#define AGX_RTKIT_BUFFER_REQUEST 1u
#define AGX_RTKIT_SYSLOG_LOG 5u
#define AGX_RTKIT_SYSLOG_INIT 8u
#define AGX_RTKIT_IOREPORT_ACK_A 0x8u
#define AGX_RTKIT_IOREPORT_ACK_B 0xcu
#define AGX_RTKIT_CRASHLOG_CRASH 1u

#define AGX_RTKIT_BUFREQ_SIZE_SHIFT 44
#define AGX_RTKIT_BUFREQ_SIZE_MASK 0xffULL
#define AGX_RTKIT_BUFREQ_IOVA_MASK 0xfffffffffffULL
#define AGX_RTKIT_OSLOG_SIZE_SHIFT 36
#define AGX_RTKIT_OSLOG_SIZE_MASK 0xfffffULL
#define AGX_RTKIT_OSLOG_IOVA_MASK 0xfffffffffULL
#define AGX_RTKIT_PAGE_SHIFT 12

#define AGX_RTKIT_BUFFER_CRASHLOG AGX_BIT64(0)
#define AGX_RTKIT_BUFFER_SYSLOG AGX_BIT64(1)
#define AGX_RTKIT_BUFFER_IOREPORT AGX_BIT64(2)
#define AGX_RTKIT_BUFFER_OSLOG AGX_BIT64(3)

struct agx_rtkit_ops {
	int (*send)(void *ctx, u32 endpoint, u64 message);
	int (*buffer)(void *ctx, u32 endpoint, u64 size, u64 requested_iova,
		      u64 *iova);
	void (*app)(void *ctx, u32 endpoint, u64 message);
	void (*crashed)(void *ctx);
};

struct agx_rtkit {
	const struct agx_rtkit_ops *ops;
	void *ctx;
	u32 endpoints[AGX_RTKIT_EP_COUNT / 32];
	u32 buffers;
	u32 iop_power;
	u32 ap_power;
	u32 version;
	u8 hello_done;
	u8 epmap_done;
	u8 iop_acked;
	u8 ap_acked;
	u8 crashed;
	int error;
	u64 rx_count;
	u64 stray_count;
	u64 unknown_count;
	u64 last_unknown;
};

AGX_BEGIN_DECLS

void agx_rtkit_init(struct agx_rtkit *rtk, const struct agx_rtkit_ops *ops,
		    void *ctx);
void agx_rtkit_rx(struct agx_rtkit *rtk, u32 endpoint, u64 message);
int agx_rtkit_set_iop_power(struct agx_rtkit *rtk, u32 state);
int agx_rtkit_set_ap_power(struct agx_rtkit *rtk, u32 state);
int agx_rtkit_start_endpoint(struct agx_rtkit *rtk, u32 endpoint);

bool agx_rtkit_has_endpoint(const struct agx_rtkit *rtk, u32 endpoint);
bool agx_rtkit_hello_done(const struct agx_rtkit *rtk);
bool agx_rtkit_epmap_done(const struct agx_rtkit *rtk);
bool agx_rtkit_iop_acked(const struct agx_rtkit *rtk);
bool agx_rtkit_ap_acked(const struct agx_rtkit *rtk);
bool agx_rtkit_crashed(const struct agx_rtkit *rtk);
bool agx_rtkit_running(const struct agx_rtkit *rtk);
int agx_rtkit_error(const struct agx_rtkit *rtk);

AGX_END_DECLS

#endif
