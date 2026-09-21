#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <vector>

#include "agx/rtkit.h"
#include "expect.hpp"
#include "fakefw.hpp"

using agxtest::expect;

namespace {

using agxtest::Msg;
using agxtest::FakeFirmware;
using agxtest::mgmt;
using agxtest::mgmt_type;

struct Harness {
	struct agx_rtkit rtk;
	FakeFirmware fw;
	int send_fail_after = -1;
	int send_count = 0;
	int buffer_error = 0;
	std::map<uint32_t, uint64_t> buffer_sizes;
	std::map<uint32_t, uint64_t> buffer_iovas;
	uint64_t next_iova = 0x10000000ULL;
	bool crashed_called = false;
	std::vector<Msg> app_msgs;
	struct agx_rtkit_ops ops;

	static int do_send(void *ctx, uint32_t ep, uint64_t msg)
	{
		Harness *h = static_cast<Harness *>(ctx);

		if (h->send_fail_after >= 0 && h->send_count >= h->send_fail_after)
			return AGX_ETIMEDOUT;
		h->send_count++;
		h->fw.handle(ep, msg);
		return 0;
	}

	static int do_buffer(void *ctx, uint32_t ep, uint64_t size,
			     uint64_t requested_iova, uint64_t *iova)
	{
		Harness *h = static_cast<Harness *>(ctx);

		(void)requested_iova;
		if (h->buffer_error)
			return h->buffer_error;
		*iova = h->next_iova;
		h->next_iova += 0x100000;
		h->buffer_sizes[ep] = size;
		h->buffer_iovas[ep] = *iova;
		return 0;
	}

	static void do_app(void *ctx, uint32_t ep, uint64_t msg)
	{
		static_cast<Harness *>(ctx)->app_msgs.push_back({ep, msg});
	}

	static void do_crashed(void *ctx)
	{
		static_cast<Harness *>(ctx)->crashed_called = true;
	}

	Harness()
	{
		ops.send = do_send;
		ops.buffer = do_buffer;
		ops.app = do_app;
		ops.crashed = do_crashed;
		agx_rtkit_init(&rtk, &ops, this);
	}

	void pump()
	{
		while (!fw.to_ap.empty()) {
			Msg m = fw.to_ap.front();
			fw.to_ap.pop_front();
			agx_rtkit_rx(&rtk, m.ep, m.data);
		}
	}

	int boot()
	{
		int ret;

		fw.power_on();
		ret = agx_rtkit_set_iop_power(&rtk, AGX_PWR_INIT);
		if (ret)
			return ret;
		pump();
		if (agx_rtkit_error(&rtk))
			return agx_rtkit_error(&rtk);
		if (!agx_rtkit_epmap_done(&rtk) || !agx_rtkit_iop_acked(&rtk))
			return AGX_ETIMEDOUT;
		ret = agx_rtkit_set_ap_power(&rtk, AGX_PWR_ON);
		if (ret)
			return ret;
		pump();
		if (!agx_rtkit_ap_acked(&rtk))
			return AGX_ETIMEDOUT;
		return 0;
	}
};

void test_full_boot()
{
	Harness h;
	int ret = h.boot();

	expect(ret == 0, "full boot completes");
	expect(agx_rtkit_error(&h.rtk) == 0, "no error recorded");
	expect(agx_rtkit_hello_done(&h.rtk), "hello handled");
	expect(agx_rtkit_epmap_done(&h.rtk), "endpoint map complete");
	expect(agx_rtkit_running(&h.rtk), "iop and ap power both on");
	expect(h.rtk.version == 12, "protocol version 12 chosen for firmware 11 to 12");

	expect(h.fw.hello_replies.size() == 1, "exactly one hello reply");
	if (!h.fw.hello_replies.empty()) {
		uint64_t r = h.fw.hello_replies[0];
		expect((r & 0xffff) == 12 && ((r >> 16) & 0xffff) == 12,
		       "hello reply carries chosen version as min and max");
	}
	expect(h.fw.from_ap.size() > 0 && h.fw.from_ap[0].ep == 0 &&
		       mgmt_type(h.fw.from_ap[0].data) == AGX_RTKIT_MGMT_IOP_PWR &&
		       (h.fw.from_ap[0].data & 0xffff) == AGX_PWR_INIT,
	       "first message is iop power INIT on the management endpoint");

	expect(h.fw.epmap_replies.size() == 2, "two endpoint map replies");
	if (h.fw.epmap_replies.size() == 2) {
		uint64_t a = h.fw.epmap_replies[0];
		uint64_t b = h.fw.epmap_replies[1];
		expect(((a >> 32) & 7) == 0 && (a & AGX_RTKIT_EPMAP_MORE) &&
			       !(a & AGX_RTKIT_EPMAP_LAST),
		       "first reply is base 0 with the more flag");
		expect(((b >> 32) & 7) == 1 && (b & AGX_RTKIT_EPMAP_LAST) &&
			       !(b & AGX_RTKIT_EPMAP_MORE),
		       "second reply is base 1 with the last flag");
	}

	std::vector<uint32_t> want = {1, 2, 3, 4, 8};
	std::vector<uint32_t> got = h.fw.started;
	std::sort(got.begin(), got.end());
	expect(got == want, "system endpoints 1 2 3 4 8 started and nothing else");

	expect(h.rtk.buffers == 0xf, "four shared buffers allocated");
	expect(h.buffer_sizes[1] == 0x2000 && h.buffer_sizes[2] == 0x2000 &&
		       h.buffer_sizes[4] == 0x2000,
	       "page count buffer requests decode to bytes");
	expect(h.buffer_sizes[8] == 0x2000, "oslog buffer request decodes its size");

	int replies_ok = 0;
	for (const Msg &m : h.fw.buffer_replies) {
		if (m.ep == 8) {
			bool ok = ((m.data >> 56) & 0xff) == 1 &&
				  ((m.data >> 36) & 0xfffff) == 0x2000 &&
				  ((m.data & 0xfffffffffULL) << 12) == h.buffer_iovas[8];
			if (ok)
				replies_ok++;
		} else if (m.ep == 1 || m.ep == 2 || m.ep == 4) {
			bool ok = ((m.data >> 52) & 0xff) == 1 &&
				  ((m.data >> 44) & 0xff) == 2 &&
				  (m.data & 0xfffffffffffULL) == h.buffer_iovas[m.ep];
			if (ok)
				replies_ok++;
		}
	}
	expect(replies_ok == 4, "all four buffer replies encode size and address correctly");

	expect(agx_rtkit_start_endpoint(&h.rtk, 0x30) != 0, "starting an undiscovered endpoint is refused");
	expect(agx_rtkit_start_endpoint(&h.rtk, AGX_RTKIT_EP_FIRMWARE) == 0, "firmware endpoint starts");
	expect(agx_rtkit_start_endpoint(&h.rtk, AGX_RTKIT_EP_DOORBELL) == 0, "doorbell endpoint starts");
	expect(std::count(h.fw.started.begin(), h.fw.started.end(), 0x20u) == 1 &&
		       std::count(h.fw.started.begin(), h.fw.started.end(), 0x21u) == 1,
	       "firmware saw one start for each application endpoint");
}

void test_app_endpoint_needs_running()
{
	Harness h;

	h.fw.power_on();
	agx_rtkit_set_iop_power(&h.rtk, AGX_PWR_INIT);
	h.pump();
	expect(agx_rtkit_start_endpoint(&h.rtk, AGX_RTKIT_EP_FIRMWARE) != 0,
	       "application endpoint refused before ap power is on");
}

void test_version_negotiation()
{
	{
		Harness h;
		h.fw.min_ver = 11;
		h.fw.max_ver = 14;
		h.boot();
		expect(h.rtk.version == 12, "firmware 11 to 14 gets version 12");
	}
	{
		Harness h;
		h.fw.min_ver = 11;
		h.fw.max_ver = 11;
		h.boot();
		expect(h.rtk.version == 11, "firmware 11 to 11 gets version 11");
		expect(h.fw.hello_replies.size() == 1 &&
			       (h.fw.hello_replies[0] & 0xffff) == 11 &&
			       ((h.fw.hello_replies[0] >> 16) & 0xffff) == 11,
		       "reply uses version 11 for both fields");
	}
	{
		Harness h;
		h.fw.min_ver = 13;
		h.fw.max_ver = 14;
		int ret = h.boot();
		expect(ret == AGX_EINVAL, "firmware requiring version 13 fails boot");
		expect(!agx_rtkit_hello_done(&h.rtk) && h.fw.hello_replies.empty(),
		       "no hello reply is sent to an incompatible firmware");
	}
	{
		Harness h;
		h.fw.min_ver = 8;
		h.fw.max_ver = 10;
		int ret = h.boot();
		expect(ret == AGX_EINVAL, "firmware only supporting up to 10 fails boot");
	}
	{
		Harness h;
		h.fw.min_ver = 12;
		h.fw.max_ver = 11;
		int ret = h.boot();
		expect(ret == AGX_EINVAL, "malformed range with min above max is refused");
	}
}

void test_traffic_after_boot()
{
	Harness h;

	h.boot();
	h.fw.to_ap.push_back({2, mgmt(AGX_RTKIT_SYSLOG_LOG, 0x07)});
	h.fw.to_ap.push_back({4, mgmt(AGX_RTKIT_IOREPORT_ACK_A, 0)});
	h.fw.to_ap.push_back({4, mgmt(AGX_RTKIT_IOREPORT_ACK_B, 0)});
	size_t before = h.fw.buffer_replies.size();
	h.pump();
	expect(h.fw.buffer_replies.size() == before + 3, "syslog log and ioreport messages are acknowledged");
	expect(h.fw.buffer_replies[before].ep == 2 &&
		       h.fw.buffer_replies[before].data == mgmt(AGX_RTKIT_SYSLOG_LOG, 0x07),
	       "syslog acknowledgement echoes the message");

	h.fw.to_ap.push_back({AGX_RTKIT_EP_FIRMWARE, 0xdeadbeefULL});
	h.pump();
	expect(h.app_msgs.size() == 1 && h.app_msgs[0].ep == 0x20 &&
		       h.app_msgs[0].data == 0xdeadbeefULL,
	       "application endpoint messages reach the app callback");

	h.fw.to_ap.push_back({0, mgmt(0xc, 0x1234)});
	h.pump();
	expect(h.rtk.unknown_count >= 1 && mgmt_type(h.rtk.last_unknown) == 0xc,
	       "management type 0xc is recorded as unknown without failing");
	expect(agx_rtkit_error(&h.rtk) == 0, "unknown messages do not raise an error");

	h.fw.to_ap.push_back({1, mgmt(AGX_RTKIT_CRASHLOG_CRASH, 0)});
	h.pump();
	expect(agx_rtkit_crashed(&h.rtk) && h.crashed_called,
	       "crashlog message after buffer setup marks a crash and calls back");
}

void test_failures()
{
	{
		Harness h;
		h.send_fail_after = 2;
		int ret = h.boot();
		expect(ret != 0 && agx_rtkit_error(&h.rtk) == AGX_ETIMEDOUT,
		       "send failure is recorded as the boot error");
	}
	{
		Harness h;
		h.buffer_error = AGX_ENOMEM;
		int ret = h.boot();
		expect(ret == AGX_ENOMEM, "buffer allocation failure fails boot with ENOMEM");
		expect(h.rtk.buffers == 0, "no buffer is recorded after a failed allocation");
	}
	{
		Harness h;
		h.ops.buffer = nullptr;
		agx_rtkit_init(&h.rtk, &h.ops, &h);
		int ret = h.boot();
		expect(ret == AGX_ENODEV, "buffer request without a buffer handler fails with ENODEV");
	}
	{
		Harness h;
		h.boot();
		int first = agx_rtkit_error(&h.rtk);
		h.fw.to_ap.push_back({2, (static_cast<uint64_t>(1) << 52)});
		h.buffer_error = AGX_EINVAL;
		h.pump();
		expect(first == 0, "clean boot has no error before the injected failure");
		expect(h.rtk.buffers == 0xf, "zero size request does not corrupt buffer bookkeeping");
	}
	{
		Harness h;
		h.boot();
		h.rtk.buffers = 0;
		h.fw.to_ap.push_back({2, mgmt(AGX_RTKIT_BUFFER_REQUEST, 0)});
		h.pump();
		expect(agx_rtkit_error(&h.rtk) == AGX_EINVAL, "zero sized buffer request is rejected");
	}
}

void test_stray_and_masking()
{
	Harness h;

	agx_rtkit_rx(&h.rtk, 0x0d, 0x1);
	expect(h.rtk.stray_count == 1, "message to an undiscovered endpoint is counted as stray");
	expect(h.rtk.unknown_count == 1, "unhandled system range endpoint is recorded as unknown");
	agx_rtkit_rx(&h.rtk, 0x77, 0x2);
	expect(h.app_msgs.size() == 1 && h.app_msgs[0].ep == 0x77,
	       "application range endpoint goes to the app callback");
	agx_rtkit_rx(&h.rtk, 0x100, mgmt(0x3, 0));
	expect(h.rtk.rx_count == 3, "endpoint above 0xff is masked and still handled");
}

uint64_t xorshift(uint64_t &s)
{
	s ^= s << 13;
	s ^= s >> 7;
	s ^= s << 17;
	return s;
}

void test_fuzz()
{
	Harness h;
	uint64_t seed = 0x9e3779b97f4a7c15ULL;
	const int rounds = 300000;

	for (int i = 0; i < rounds; i++) {
		uint32_t ep = static_cast<uint32_t>(xorshift(seed) & 0x1ff);
		uint64_t msg = xorshift(seed);
		if ((i & 7) == 0)
			msg &= ~(0xffULL << 52);
		if ((i & 15) == 0)
			msg |= (static_cast<uint64_t>(i & 0xf) << 52);
		agx_rtkit_rx(&h.rtk, ep, msg);
	}
	expect(h.rtk.rx_count == static_cast<uint64_t>(rounds),
	       "fuzz run handled every random message without crashing");
}

void test_flags_reset()
{
	Harness h;

	h.boot();
	expect(agx_rtkit_iop_acked(&h.rtk), "iop ack flag set after boot");
	agx_rtkit_set_iop_power(&h.rtk, AGX_PWR_SLEEP);
	expect(!agx_rtkit_iop_acked(&h.rtk), "iop ack flag cleared when a new power request goes out");
	agx_rtkit_set_ap_power(&h.rtk, AGX_PWR_QUIESCED);
	expect(!agx_rtkit_ap_acked(&h.rtk), "ap ack flag cleared when a new power request goes out");
}

}

int main()
{
	test_full_boot();
	test_app_endpoint_needs_running();
	test_version_negotiation();
	test_traffic_after_boot();
	test_failures();
	test_stray_and_masking();
	test_fuzz();
	test_flags_reset();
	return agxtest::finish();
}
