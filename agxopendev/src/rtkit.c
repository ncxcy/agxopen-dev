#include "agx/rtkit.h"

static u64 agx_rtkit_mgmt_msg(u32 type, u64 payload)
{
	payload &= ~(AGX_RTKIT_TYPE_MASK << AGX_RTKIT_TYPE_SHIFT);
	return payload |
	       AGX_FIELD_PUT(type, AGX_RTKIT_TYPE_SHIFT, AGX_RTKIT_TYPE_MASK);
}

static void agx_rtkit_fail(struct agx_rtkit *rtk, int error)
{
	int expected = 0;

	__atomic_compare_exchange_n(&rtk->error, &expected, error, 0,
				    __ATOMIC_RELEASE, __ATOMIC_RELAXED);
}

static int agx_rtkit_send(struct agx_rtkit *rtk, u32 endpoint, u64 message)
{
	int ret = rtk->ops->send(rtk->ctx, endpoint, message);

	if (ret)
		agx_rtkit_fail(rtk, ret);
	return ret;
}

static int agx_rtkit_mgmt_send(struct agx_rtkit *rtk, u32 type, u64 payload)
{
	return agx_rtkit_send(rtk, AGX_RTKIT_EP_MGMT,
			      agx_rtkit_mgmt_msg(type, payload));
}

static void agx_rtkit_note_unknown(struct agx_rtkit *rtk, u64 message)
{
	rtk->unknown_count++;
	rtk->last_unknown = message;
}

static u32 agx_rtkit_buffer_bit(u32 endpoint)
{
	switch (endpoint) {
	case AGX_RTKIT_EP_CRASHLOG:
		return (u32)AGX_RTKIT_BUFFER_CRASHLOG;
	case AGX_RTKIT_EP_SYSLOG:
		return (u32)AGX_RTKIT_BUFFER_SYSLOG;
	case AGX_RTKIT_EP_IOREPORT:
		return (u32)AGX_RTKIT_BUFFER_IOREPORT;
	case AGX_RTKIT_EP_OSLOG:
		return (u32)AGX_RTKIT_BUFFER_OSLOG;
	default:
		return 0;
	}
}

void agx_rtkit_init(struct agx_rtkit *rtk, const struct agx_rtkit_ops *ops,
		    void *ctx)
{
	u32 i;

	rtk->ops = ops;
	rtk->ctx = ctx;
	for (i = 0; i < AGX_RTKIT_EP_COUNT / 32; i++)
		rtk->endpoints[i] = 0;
	rtk->endpoints[0] = 1u;
	rtk->buffers = 0;
	rtk->iop_power = AGX_PWR_OFF;
	rtk->ap_power = AGX_PWR_OFF;
	rtk->version = 0;
	rtk->hello_done = 0;
	rtk->epmap_done = 0;
	rtk->iop_acked = 0;
	rtk->ap_acked = 0;
	rtk->crashed = 0;
	rtk->error = 0;
	rtk->rx_count = 0;
	rtk->stray_count = 0;
	rtk->unknown_count = 0;
	rtk->last_unknown = 0;
}

bool agx_rtkit_has_endpoint(const struct agx_rtkit *rtk, u32 endpoint)
{
	if (endpoint >= AGX_RTKIT_EP_COUNT)
		return false;
	return ((AGX_LOAD_ACQ(rtk->endpoints[endpoint / 32]) >>
		 (endpoint % 32)) & 1u) != 0;
}

bool agx_rtkit_hello_done(const struct agx_rtkit *rtk)
{
	return AGX_LOAD_ACQ(rtk->hello_done) != 0;
}

bool agx_rtkit_epmap_done(const struct agx_rtkit *rtk)
{
	return AGX_LOAD_ACQ(rtk->epmap_done) != 0;
}

bool agx_rtkit_iop_acked(const struct agx_rtkit *rtk)
{
	return AGX_LOAD_ACQ(rtk->iop_acked) != 0;
}

bool agx_rtkit_ap_acked(const struct agx_rtkit *rtk)
{
	return AGX_LOAD_ACQ(rtk->ap_acked) != 0;
}

bool agx_rtkit_crashed(const struct agx_rtkit *rtk)
{
	return AGX_LOAD_ACQ(rtk->crashed) != 0;
}

bool agx_rtkit_running(const struct agx_rtkit *rtk)
{
	return (AGX_LOAD_ACQ(rtk->iop_power) & AGX_PWR_LEVEL_MASK) ==
		       AGX_PWR_ON &&
	       (AGX_LOAD_ACQ(rtk->ap_power) & AGX_PWR_LEVEL_MASK) ==
		       AGX_PWR_ON;
}

int agx_rtkit_error(const struct agx_rtkit *rtk)
{
	return AGX_LOAD_ACQ(rtk->error);
}

int agx_rtkit_set_iop_power(struct agx_rtkit *rtk, u32 state)
{
	AGX_STORE_REL(rtk->iop_acked, 0);
	return agx_rtkit_mgmt_send(rtk, AGX_RTKIT_MGMT_IOP_PWR,
				   AGX_FIELD_PUT(state, 0,
						 AGX_RTKIT_PWR_STATE_MASK));
}

int agx_rtkit_set_ap_power(struct agx_rtkit *rtk, u32 state)
{
	AGX_STORE_REL(rtk->ap_acked, 0);
	return agx_rtkit_mgmt_send(rtk, AGX_RTKIT_MGMT_AP_PWR,
				   AGX_FIELD_PUT(state, 0,
						 AGX_RTKIT_PWR_STATE_MASK));
}

int agx_rtkit_start_endpoint(struct agx_rtkit *rtk, u32 endpoint)
{
	u64 payload;

	if (endpoint >= AGX_RTKIT_EP_COUNT)
		return AGX_EINVAL;
	if (!agx_rtkit_has_endpoint(rtk, endpoint))
		return AGX_EINVAL;
	if (endpoint >= AGX_RTKIT_EP_APP_START && !agx_rtkit_running(rtk))
		return AGX_EINVAL;

	payload = AGX_FIELD_PUT(endpoint, AGX_RTKIT_STARTEP_EP_SHIFT,
				AGX_RTKIT_STARTEP_EP_MASK) |
		  AGX_RTKIT_STARTEP_FLAG;
	return agx_rtkit_mgmt_send(rtk, AGX_RTKIT_MGMT_STARTEP, payload);
}

static void agx_rtkit_rx_hello(struct agx_rtkit *rtk, u64 message)
{
	u32 min_ver = (u32)AGX_FIELD_GET(message, AGX_RTKIT_HELLO_MINVER_SHIFT,
					 AGX_RTKIT_HELLO_VER_MASK);
	u32 max_ver = (u32)AGX_FIELD_GET(message, AGX_RTKIT_HELLO_MAXVER_SHIFT,
					 AGX_RTKIT_HELLO_VER_MASK);
	u32 want;
	u64 reply;

	if (min_ver > max_ver || min_ver > AGX_RTKIT_PROTO_MAX ||
	    max_ver < AGX_RTKIT_PROTO_MIN) {
		agx_rtkit_fail(rtk, AGX_EINVAL);
		return;
	}

	want = max_ver < AGX_RTKIT_PROTO_MAX ? max_ver : AGX_RTKIT_PROTO_MAX;
	rtk->version = want;

	reply = AGX_FIELD_PUT(want, AGX_RTKIT_HELLO_MINVER_SHIFT,
			      AGX_RTKIT_HELLO_VER_MASK) |
		AGX_FIELD_PUT(want, AGX_RTKIT_HELLO_MAXVER_SHIFT,
			      AGX_RTKIT_HELLO_VER_MASK);
	if (agx_rtkit_mgmt_send(rtk, AGX_RTKIT_MGMT_HELLO_REPLY, reply))
		return;
	AGX_STORE_REL(rtk->hello_done, 1);
}

static void agx_rtkit_rx_epmap(struct agx_rtkit *rtk, u64 message)
{
	u32 base = (u32)AGX_FIELD_GET(message, AGX_RTKIT_EPMAP_BASE_SHIFT,
				      AGX_RTKIT_EPMAP_BASE_MASK);
	u32 bitmap = (u32)AGX_FIELD_GET(message, 0,
					AGX_RTKIT_EPMAP_BITMAP_MASK);
	u64 reply = AGX_FIELD_PUT(base, AGX_RTKIT_EPMAP_BASE_SHIFT,
				  AGX_RTKIT_EPMAP_BASE_MASK);
	u32 endpoint;

	AGX_OR_REL(rtk->endpoints[base], bitmap);

	reply |= (message & AGX_RTKIT_EPMAP_LAST) ? AGX_RTKIT_EPMAP_LAST :
						    AGX_RTKIT_EPMAP_MORE;
	if (agx_rtkit_mgmt_send(rtk, AGX_RTKIT_MGMT_EPMAP_REPLY, reply))
		return;
	if (!(message & AGX_RTKIT_EPMAP_LAST))
		return;

	for (endpoint = 1; endpoint < AGX_RTKIT_EP_APP_START; endpoint++) {
		if (!agx_rtkit_has_endpoint(rtk, endpoint))
			continue;

		switch (endpoint) {
		case AGX_RTKIT_EP_CRASHLOG:
		case AGX_RTKIT_EP_SYSLOG:
		case AGX_RTKIT_EP_DEBUG:
		case AGX_RTKIT_EP_IOREPORT:
		case AGX_RTKIT_EP_OSLOG:
			if (agx_rtkit_start_endpoint(rtk, endpoint))
				return;
			break;
		default:
			rtk->unknown_count++;
			break;
		}
	}

	AGX_STORE_REL(rtk->epmap_done, 1);
}

static void agx_rtkit_rx_mgmt(struct agx_rtkit *rtk, u64 message)
{
	u32 type = (u32)AGX_FIELD_GET(message, AGX_RTKIT_TYPE_SHIFT,
				      AGX_RTKIT_TYPE_MASK);
	u32 state = (u32)AGX_FIELD_GET(message, 0, AGX_RTKIT_PWR_STATE_MASK);

	switch (type) {
	case AGX_RTKIT_MGMT_HELLO:
		agx_rtkit_rx_hello(rtk, message);
		break;
	case AGX_RTKIT_MGMT_EPMAP:
		agx_rtkit_rx_epmap(rtk, message);
		break;
	case AGX_RTKIT_MGMT_IOP_PWR_ACK:
		AGX_STORE_REL(rtk->iop_power, state);
		AGX_STORE_REL(rtk->iop_acked, 1);
		break;
	case AGX_RTKIT_MGMT_AP_PWR_ACK:
		AGX_STORE_REL(rtk->ap_power, state);
		AGX_STORE_REL(rtk->ap_acked, 1);
		break;
	default:
		agx_rtkit_note_unknown(rtk, message);
		break;
	}
}

static void agx_rtkit_get_buffer(struct agx_rtkit *rtk, u32 endpoint,
				 u64 message)
{
	u64 size;
	u64 iova;
	u64 reply;
	int ret;

	if (endpoint == AGX_RTKIT_EP_OSLOG) {
		size = AGX_FIELD_GET(message, AGX_RTKIT_OSLOG_SIZE_SHIFT,
				     AGX_RTKIT_OSLOG_SIZE_MASK);
		iova = AGX_FIELD_GET(message, 0, AGX_RTKIT_OSLOG_IOVA_MASK)
		       << AGX_RTKIT_PAGE_SHIFT;
	} else {
		size = AGX_FIELD_GET(message, AGX_RTKIT_BUFREQ_SIZE_SHIFT,
				     AGX_RTKIT_BUFREQ_SIZE_MASK)
		       << AGX_RTKIT_PAGE_SHIFT;
		iova = AGX_FIELD_GET(message, 0, AGX_RTKIT_BUFREQ_IOVA_MASK);
	}

	if (size == 0 || !rtk->ops->buffer) {
		agx_rtkit_fail(rtk, size == 0 ? AGX_EINVAL : AGX_ENODEV);
		return;
	}

	ret = rtk->ops->buffer(rtk->ctx, endpoint, size, iova, &iova);
	if (ret) {
		agx_rtkit_fail(rtk, ret);
		return;
	}

	rtk->buffers |= agx_rtkit_buffer_bit(endpoint);

	if (endpoint == AGX_RTKIT_EP_OSLOG) {
		reply = AGX_FIELD_PUT(AGX_RTKIT_BUFFER_REQUEST,
				      AGX_RTKIT_OSLOG_TYPE_SHIFT,
				      AGX_RTKIT_TYPE_MASK) |
			AGX_FIELD_PUT(size, AGX_RTKIT_OSLOG_SIZE_SHIFT,
				      AGX_RTKIT_OSLOG_SIZE_MASK) |
			AGX_FIELD_PUT(iova >> AGX_RTKIT_PAGE_SHIFT, 0,
				      AGX_RTKIT_OSLOG_IOVA_MASK);
	} else {
		reply = AGX_FIELD_PUT(AGX_RTKIT_BUFFER_REQUEST,
				      AGX_RTKIT_TYPE_SHIFT,
				      AGX_RTKIT_TYPE_MASK) |
			AGX_FIELD_PUT(size >> AGX_RTKIT_PAGE_SHIFT,
				      AGX_RTKIT_BUFREQ_SIZE_SHIFT,
				      AGX_RTKIT_BUFREQ_SIZE_MASK) |
			AGX_FIELD_PUT(iova, 0, AGX_RTKIT_BUFREQ_IOVA_MASK);
	}

	agx_rtkit_send(rtk, endpoint, reply);
}

static void agx_rtkit_rx_crashlog(struct agx_rtkit *rtk, u64 message)
{
	u32 type = (u32)AGX_FIELD_GET(message, AGX_RTKIT_TYPE_SHIFT,
				      AGX_RTKIT_TYPE_MASK);

	if (type != AGX_RTKIT_CRASHLOG_CRASH) {
		agx_rtkit_note_unknown(rtk, message);
		return;
	}

	if (!(rtk->buffers & (u32)AGX_RTKIT_BUFFER_CRASHLOG)) {
		agx_rtkit_get_buffer(rtk, AGX_RTKIT_EP_CRASHLOG, message);
		return;
	}

	AGX_STORE_REL(rtk->crashed, 1);
	if (rtk->ops->crashed)
		rtk->ops->crashed(rtk->ctx);
}

static void agx_rtkit_rx_syslog(struct agx_rtkit *rtk, u64 message)
{
	u32 type = (u32)AGX_FIELD_GET(message, AGX_RTKIT_TYPE_SHIFT,
				      AGX_RTKIT_TYPE_MASK);

	switch (type) {
	case AGX_RTKIT_BUFFER_REQUEST:
		agx_rtkit_get_buffer(rtk, AGX_RTKIT_EP_SYSLOG, message);
		break;
	case AGX_RTKIT_SYSLOG_INIT:
		break;
	case AGX_RTKIT_SYSLOG_LOG:
		agx_rtkit_send(rtk, AGX_RTKIT_EP_SYSLOG, message);
		break;
	default:
		agx_rtkit_note_unknown(rtk, message);
		break;
	}
}

static void agx_rtkit_rx_ioreport(struct agx_rtkit *rtk, u64 message)
{
	u32 type = (u32)AGX_FIELD_GET(message, AGX_RTKIT_TYPE_SHIFT,
				      AGX_RTKIT_TYPE_MASK);

	switch (type) {
	case AGX_RTKIT_BUFFER_REQUEST:
		agx_rtkit_get_buffer(rtk, AGX_RTKIT_EP_IOREPORT, message);
		break;
	case AGX_RTKIT_IOREPORT_ACK_A:
	case AGX_RTKIT_IOREPORT_ACK_B:
		agx_rtkit_send(rtk, AGX_RTKIT_EP_IOREPORT, message);
		break;
	default:
		agx_rtkit_note_unknown(rtk, message);
		break;
	}
}

static void agx_rtkit_rx_oslog(struct agx_rtkit *rtk, u64 message)
{
	u32 type = (u32)AGX_FIELD_GET(message, AGX_RTKIT_OSLOG_TYPE_SHIFT,
				      AGX_RTKIT_TYPE_MASK);

	if (type == AGX_RTKIT_BUFFER_REQUEST)
		agx_rtkit_get_buffer(rtk, AGX_RTKIT_EP_OSLOG, message);
	else
		agx_rtkit_note_unknown(rtk, message);
}

void agx_rtkit_rx(struct agx_rtkit *rtk, u32 endpoint, u64 message)
{
	endpoint &= 0xffu;
	rtk->rx_count++;

	if (!agx_rtkit_has_endpoint(rtk, endpoint))
		rtk->stray_count++;

	switch (endpoint) {
	case AGX_RTKIT_EP_MGMT:
		agx_rtkit_rx_mgmt(rtk, message);
		break;
	case AGX_RTKIT_EP_CRASHLOG:
		agx_rtkit_rx_crashlog(rtk, message);
		break;
	case AGX_RTKIT_EP_SYSLOG:
		agx_rtkit_rx_syslog(rtk, message);
		break;
	case AGX_RTKIT_EP_IOREPORT:
		agx_rtkit_rx_ioreport(rtk, message);
		break;
	case AGX_RTKIT_EP_OSLOG:
		agx_rtkit_rx_oslog(rtk, message);
		break;
	default:
		if (endpoint >= AGX_RTKIT_EP_APP_START && rtk->ops->app)
			rtk->ops->app(rtk->ctx, endpoint, message);
		else
			agx_rtkit_note_unknown(rtk, message);
		break;
	}
}
