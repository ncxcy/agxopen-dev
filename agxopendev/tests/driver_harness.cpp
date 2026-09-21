#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <atomic>
#include <utility>
#include <vector>

extern "C" {
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include "agx/gpu_ep.h"
#include "agx/mailbox_regs.h"
#include "agx/rtkit.h"
#include "display.h"
#include "rtbuddy.h"
}

#include "expect.hpp"
#include "fakefw.hpp"

using agxtest::expect;
using agxtest::mgmt;
using agxtest::mgmt_type;
using agxtest::Msg;

namespace {

const uintptr_t kBase = 0x500000000ULL;
const uint32_t kCpuCtrlOffset = 0x44;
const uint32_t kCpuCtrlRun = 1u << 4;

struct FakeAsc {
	std::mutex m;
	std::condition_variable cv;
	std::deque<Msg> a2i;
	std::deque<Msg> i2a;
	size_t a2i_capacity = 8;
	uint32_t cpu_ctrl = 0;
	uint64_t send0 = 0;
	bool send0_written = false;
	bool recv0_read = false;
	bool outbox_enable = false;
	int enable_order_violation = 0;
	bool fw_running = false;
	bool fw_silent = false;
	bool fw_ignore = false;
	int fw_delay_us = 0;
	bool stop = false;
	int bad_access = 0;
	int overflow = 0;
	int order_violation = 0;
	int run_set_count = 0;
	int run_clear_count = 0;
	agxtest::FakeFirmware fw;
	std::thread fw_thread;

	bool irq_stop = false;
	std::thread irq_thread;

	bool visible_locked() const
	{
		return outbox_enable && !i2a.empty();
	}

	void flush_locked()
	{
		while (!fw.to_ap.empty()) {
			i2a.push_back(fw.to_ap.front());
			fw.to_ap.pop_front();
		}
		cv.notify_all();
	}

	void fw_loop()
	{
		std::unique_lock<std::mutex> lk(m);

		while (!stop) {
			cv.wait(lk, [&] {
				return stop || (fw_running && !fw_ignore && !a2i.empty());
			});
			if (stop)
				break;
			while (!a2i.empty() && fw_running && !fw_ignore) {
				Msg msg = a2i.front();
				a2i.pop_front();
				fw.handle(msg.ep, msg.data);
				flush_locked();
				if (fw_delay_us) {
					lk.unlock();
					usleep(fw_delay_us);
					lk.lock();
				}
			}
			cv.notify_all();
		}
	}

	void push_to_ap(uint32_t ep, uint64_t data)
	{
		std::lock_guard<std::mutex> g(m);
		i2a.push_back({ep, data});
		cv.notify_all();
	}
};

FakeAsc *g_asc = nullptr;

std::mutex g_log_m;
std::vector<std::string> g_logs;
int g_error_logs = 0;
bool g_verbose = false;

std::mutex g_dma_m;
std::map<void *, dma_addr_t> g_dma;
dma_addr_t g_next_iova = 0x8000000ULL;

uint64_t reg_offset(const volatile void *addr)
{
	return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(addr) - kBase);
}

}

extern "C" {

u32 readl(const volatile void *addr)
{
	FakeAsc &d = *g_asc;
	std::lock_guard<std::mutex> g(d.m);

	switch (reg_offset(addr)) {
	case AGX_MBOX_A2I_CONTROL: {
		u32 v = 0;
		if (d.a2i.size() >= d.a2i_capacity)
			v |= AGX_MBOX_CONTROL_FULL;
		if (d.a2i.empty())
			v |= AGX_MBOX_CONTROL_EMPTY;
		return v;
	}
	case AGX_MBOX_I2A_CONTROL:
		return (d.outbox_enable ? AGX_MBOX_CONTROL_ENABLE : 0u) |
		       (d.visible_locked() ? 0u : AGX_MBOX_CONTROL_EMPTY);
	case kCpuCtrlOffset:
		return d.cpu_ctrl;
	default:
		d.bad_access++;
		return 0;
	}
}

u64 readq(const volatile void *addr)
{
	FakeAsc &d = *g_asc;
	std::lock_guard<std::mutex> g(d.m);

	switch (reg_offset(addr)) {
	case AGX_MBOX_I2A_RECV0:
		if (!d.visible_locked()) {
			d.bad_access++;
			return 0;
		}
		d.recv0_read = true;
		return d.i2a.front().data;
	case AGX_MBOX_I2A_RECV1: {
		if (!d.visible_locked()) {
			d.bad_access++;
			return 0;
		}
		if (!d.recv0_read)
			d.order_violation++;
		d.recv0_read = false;
		Msg msg = d.i2a.front();
		d.i2a.pop_front();
		return msg.ep;
	}
	default:
		d.bad_access++;
		return 0;
	}
}

void writel(u32 value, volatile void *addr)
{
	FakeAsc &d = *g_asc;
	std::lock_guard<std::mutex> g(d.m);

	if (reg_offset(addr) == AGX_MBOX_I2A_CONTROL) {
		d.outbox_enable = (value & AGX_MBOX_CONTROL_ENABLE) != 0;
		d.cv.notify_all();
		return;
	}

	if (reg_offset(addr) != kCpuCtrlOffset) {
		d.bad_access++;
		return;
	}

	bool was = (d.cpu_ctrl & kCpuCtrlRun) != 0;
	bool now = (value & kCpuCtrlRun) != 0;
	d.cpu_ctrl = value;

	if (!was && now) {
		d.run_set_count++;
		if (!d.outbox_enable)
			d.enable_order_violation++;
		d.fw_running = true;
		if (!d.fw_silent)
			d.fw.power_on();
		d.flush_locked();
	} else if (was && !now) {
		d.run_clear_count++;
		d.fw_running = false;
	}
	d.cv.notify_all();
}

void writeq(u64 value, volatile void *addr)
{
	FakeAsc &d = *g_asc;
	std::lock_guard<std::mutex> g(d.m);

	switch (reg_offset(addr)) {
	case AGX_MBOX_A2I_SEND0:
		d.send0 = value;
		d.send0_written = true;
		break;
	case AGX_MBOX_A2I_SEND1:
		if (!d.send0_written)
			d.order_violation++;
		d.send0_written = false;
		if (d.a2i.size() >= d.a2i_capacity) {
			d.overflow++;
			break;
		}
		d.a2i.push_back({static_cast<uint32_t>(value & 0xff), d.send0});
		d.cv.notify_all();
		break;
	default:
		d.bad_access++;
		break;
	}
}

void kshim_log(const struct device *dev, const char *level, const char *fmt, ...)
{
	char buf[512];
	va_list ap;

	(void)dev;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	std::lock_guard<std::mutex> g(g_log_m);
	g_logs.push_back(std::string(level) + ": " + buf);
	if (std::strcmp(level, "err") == 0)
		g_error_logs++;
	if (g_verbose)
		std::fprintf(stderr, "[%s] %s", level, buf);
}

int devm_request_threaded_irq(struct device *dev, unsigned int irq,
			      irq_handler_t handler, irq_handler_t thread_fn,
			      unsigned long irqflags, const char *devname,
			      void *dev_id)
{
	FakeAsc &d = *g_asc;

	(void)dev;
	(void)irqflags;
	(void)devname;
	d.irq_stop = false;
	d.irq_thread = std::thread([&d, irq, handler, thread_fn, dev_id] {
		std::unique_lock<std::mutex> lk(d.m);

		while (!d.irq_stop) {
			d.cv.wait(lk, [&] { return d.irq_stop || d.visible_locked(); });
			if (d.irq_stop)
				break;
			lk.unlock();
			irqreturn_t r = handler(static_cast<int>(irq), dev_id);
			if (r == IRQ_WAKE_THREAD)
				thread_fn(static_cast<int>(irq), dev_id);
			else if (r == IRQ_NONE)
				usleep(200);
			lk.lock();
		}
	});
	return 0;
}

void devm_free_irq(struct device *dev, unsigned int irq, void *dev_id)
{
	FakeAsc &d = *g_asc;

	(void)dev;
	(void)irq;
	(void)dev_id;
	{
		std::lock_guard<std::mutex> g(d.m);
		d.irq_stop = true;
	}
	d.cv.notify_all();
	if (d.irq_thread.joinable())
		d.irq_thread.join();
}

void *dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *handle,
			 gfp_t gfp)
{
	void *p = std::calloc(1, size);

	(void)dev;
	(void)gfp;
	std::lock_guard<std::mutex> g(g_dma_m);
	*handle = g_next_iova;
	g_next_iova += (size + 0xfffffULL) & ~0xfffffULL;
	g_dma[p] = *handle;
	return p;
}

void dma_free_coherent(struct device *dev, size_t size, void *cpu,
		       dma_addr_t handle)
{
	(void)dev;
	(void)size;
	(void)handle;
	std::lock_guard<std::mutex> g(g_dma_m);
	g_dma.erase(cpu);
	std::free(cpu);
}

}

namespace {

size_t dma_outstanding()
{
	std::lock_guard<std::mutex> g(g_dma_m);
	return g_dma.size();
}

struct Rig {
	FakeAsc asc;
	struct device dev;
	struct agx_rtbuddy rb;
	struct agx_display_broker broker;
	struct agx_display_listener listener;
	std::mutex tm;
	std::vector<std::pair<int, int>> transitions;

	static void on_power(void *cookie, enum agx_power_state from,
			     enum agx_power_state to)
	{
		Rig *r = static_cast<Rig *>(cookie);
		std::lock_guard<std::mutex> g(r->tm);
		r->transitions.push_back({static_cast<int>(from), static_cast<int>(to)});
	}

	Rig()
	{
		dev.name = "fake";
		g_asc = &asc;
		{
			std::lock_guard<std::mutex> g(g_log_m);
			g_logs.clear();
			g_error_logs = 0;
		}
		std::memset(&rb, 0xab, sizeof(rb));
		asc.fw_thread = std::thread([this] { asc.fw_loop(); });
	}

	~Rig()
	{
		{
			std::lock_guard<std::mutex> g(asc.m);
			asc.stop = true;
			asc.irq_stop = true;
		}
		asc.cv.notify_all();
		if (asc.irq_thread.joinable())
			asc.irq_thread.join();
		if (asc.fw_thread.joinable())
			asc.fw_thread.join();
		g_asc = nullptr;
	}

	int probe()
	{
		agx_display_broker_init(&broker);
		listener.callback = on_power;
		listener.cookie = this;
		agx_display_register(&broker, &listener);
		return agx_rtbuddy_probe(&rb, &dev, reinterpret_cast<void *>(kBase), 7,
					 &broker);
	}

	template <class F>
	auto with_fw(F f) -> decltype(f(std::declval<agxtest::FakeFirmware &>()))
	{
		std::lock_guard<std::mutex> g(asc.m);
		return f(asc.fw);
	}

	template <class F>
	bool wait_for(F pred, int timeout_ms = 2000)
	{
		auto end = std::chrono::steady_clock::now() +
			   std::chrono::milliseconds(timeout_ms);
		while (std::chrono::steady_clock::now() < end) {
			{
				std::lock_guard<std::mutex> g(asc.m);
				if (pred(asc))
					return true;
			}
			usleep(1000);
		}
		return false;
	}
};

long elapsed_ms(std::chrono::steady_clock::time_point start)
{
	return static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(
					 std::chrono::steady_clock::now() - start)
					 .count());
}

int find_mgmt(const std::vector<Msg> &log, uint32_t type, uint32_t state,
	      bool match_state)
{
	for (size_t i = 0; i < log.size(); i++) {
		if (log[i].ep != 0 || mgmt_type(log[i].data) != type)
			continue;
		if (!match_state || (log[i].data & 0xffff) == state)
			return static_cast<int>(i);
	}
	return -1;
}

int find_startep(const std::vector<Msg> &log, uint32_t ep)
{
	for (size_t i = 0; i < log.size(); i++) {
		if (log[i].ep == 0 &&
		    mgmt_type(log[i].data) == AGX_RTKIT_MGMT_STARTEP &&
		    ((log[i].data >> 32) & 0xff) == ep)
			return static_cast<int>(i);
	}
	return -1;
}

void test_happy_path()
{
	Rig r;

	expect(r.probe() == 0, "probe succeeds on a poisoned struct");
	expect(!agx_rtbuddy_started(&r.rb), "not started before boot");
	int ret = agx_rtbuddy_boot(&r.rb);
	expect(ret == 0, "boot succeeds against the fake coprocessor");
	expect(agx_rtbuddy_started(&r.rb), "driver reports started");
	expect(r.rb.power == AGX_POWER_ON, "power state is on");
	expect(r.rb.rtkit.version == 12, "protocol version 12 negotiated");

	bool settled = r.wait_for([](FakeAsc &a) {
		return a.fw.started.size() >= 7 && a.fw.buffer_replies.size() >= 4 &&
		       a.a2i.empty();
	});
	expect(settled, "coprocessor received every start and buffer reply");

	r.with_fw([&](agxtest::FakeFirmware &fw) {
		const std::vector<Msg> &log = fw.from_ap;
		expect(!log.empty() && log[0].ep == 0 &&
			       mgmt_type(log[0].data) == AGX_RTKIT_MGMT_IOP_PWR &&
			       (log[0].data & 0xffff) == AGX_PWR_INIT,
		       "first message on the wire is iop power INIT");
		int ap_on = find_mgmt(log, AGX_RTKIT_MGMT_AP_PWR, AGX_PWR_ON, true);
		int s20 = find_startep(log, 0x20);
		int s21 = find_startep(log, 0x21);
		expect(ap_on >= 0 && s20 > ap_on && s21 > ap_on,
		       "application endpoints start after ap power on");
		bool systems_first = true;
		for (uint32_t ep : {1u, 2u, 3u, 4u, 8u}) {
			int i = find_startep(log, ep);
			if (i < 0 || (ap_on >= 0 && i > ap_on))
				systems_first = false;
		}
		expect(systems_first, "system endpoints start before ap power on");
		expect(fw.hello_replies.size() == 1, "one hello reply on the wire");
		expect(fw.epmap_replies.size() == 2, "two endpoint map replies on the wire");
		expect(fw.buffer_replies.size() == 4, "four buffer replies on the wire");
		return 0;
	});

	for (int i = 0; i < 1000 && dma_outstanding() != 4; i++)
		usleep(1000);
	expect(dma_outstanding() == 4, "four shared buffers are live");
	expect(r.asc.run_set_count == 1, "cpu run bit set exactly once");

	agx_rtbuddy_remove(&r.rb);

	r.with_fw([&](agxtest::FakeFirmware &fw) {
		const std::vector<Msg> &log = fw.from_ap;
		int ap_q = find_mgmt(log, AGX_RTKIT_MGMT_AP_PWR, AGX_PWR_QUIESCED, true);
		int iop_s = find_mgmt(log, AGX_RTKIT_MGMT_IOP_PWR, AGX_PWR_SLEEP, true);
		expect(ap_q >= 0 && iop_s > ap_q,
		       "shutdown quiesces ap power then sleeps the iop");
		return 0;
	});

	expect(r.asc.run_clear_count == 1, "cpu run bit cleared on remove");
	expect(dma_outstanding() == 0, "all shared buffers freed on remove");
	expect(r.rb.power == AGX_POWER_OFF, "power state is off after remove");
	expect(r.rb.drain_count > 0, "interrupt thread drained messages");
	expect(r.transitions.size() == 2 && r.transitions[0].first == AGX_POWER_OFF &&
		       r.transitions[0].second == AGX_POWER_ON &&
		       r.transitions[1].first == AGX_POWER_ON &&
		       r.transitions[1].second == AGX_POWER_OFF,
	       "display listener saw off to on then on to off");
	expect(r.asc.enable_order_violation == 0,
	       "outbox was enabled before the cpu run bit was set");
	expect(r.asc.bad_access == 0, "no access outside the documented registers");
	expect(r.asc.overflow == 0, "driver never wrote to a full mailbox");
	expect(r.asc.order_violation == 0, "register access order is correct");
	expect(g_error_logs == 0, "no error messages logged on the happy path");
}

void test_outbox_gating_model()
{
	Rig r;
	struct agx_mailbox mbox;
	struct agx_mbox_message msg;
	char *base = reinterpret_cast<char *>(kBase);

	agx_mailbox_init(&mbox, base);
	r.asc.push_to_ap(AGX_RTKIT_EP_FIRMWARE, 0x1234);

	u32 before = readl(base + AGX_MBOX_I2A_CONTROL);
	expect(agx_mbox_control_empty(before) && !(before & AGX_MBOX_CONTROL_ENABLE),
	       "fake hardware hides queued messages while the outbox is disabled");
	expect(agx_mailbox_recv(&mbox, &msg) == -ENODATA,
	       "recv reports no data while the outbox is disabled");

	agx_mailbox_set_outbox_enable(&mbox, true);
	u32 after = readl(base + AGX_MBOX_I2A_CONTROL);
	expect(!agx_mbox_control_empty(after) && (after & AGX_MBOX_CONTROL_ENABLE),
	       "enabling the outbox exposes the queued message and keeps the enable bit");
	expect(agx_mailbox_recv(&mbox, &msg) == 0 && msg.endpoint == AGX_RTKIT_EP_FIRMWARE,
	       "recv returns the queued message once the outbox is enabled");
	expect(r.asc.order_violation == 0 && r.asc.bad_access == 0,
	       "gating test used only documented registers in the right order");

	agx_mailbox_set_outbox_enable(&mbox, false);
	expect(!(readl(base + AGX_MBOX_I2A_CONTROL) & AGX_MBOX_CONTROL_ENABLE),
	       "disabling clears the enable bit");
}

void test_polling_when_full()
{
	Rig r;

	r.asc.a2i_capacity = 1;
	r.asc.fw_delay_us = 3000;
	expect(r.probe() == 0, "probe with a one entry mailbox");
	int ret = agx_rtbuddy_boot(&r.rb);
	expect(ret == 0, "boot succeeds through a slow one entry mailbox");
	agx_rtbuddy_remove(&r.rb);
	expect(r.asc.overflow == 0, "sender waited instead of overflowing the mailbox");
	expect(r.asc.order_violation == 0, "no interleaved sends");
}

void test_silent_firmware()
{
	Rig r;

	r.asc.fw_silent = true;
	expect(r.probe() == 0, "probe with a silent coprocessor");
	auto start = std::chrono::steady_clock::now();
	int ret = agx_rtbuddy_boot(&r.rb);
	long ms = elapsed_ms(start);
	expect(ret == -ETIMEDOUT, "boot times out with ETIMEDOUT");
	expect(ms >= 400 && ms < 2000, "boot gives up after about the stage timeout");
	expect(r.asc.run_clear_count == 1, "cpu is stopped after a failed boot");
	expect(!agx_rtbuddy_started(&r.rb), "not started after a failed boot");
	agx_rtbuddy_remove(&r.rb);
	expect(dma_outstanding() == 0, "no buffers leaked after a failed boot");
	expect(r.rb.power == AGX_POWER_OFF, "power state stays off");
}

void test_incompatible_version()
{
	Rig r;

	r.with_fw([](agxtest::FakeFirmware &fw) {
		fw.min_ver = 13;
		fw.max_ver = 14;
		return 0;
	});
	expect(r.probe() == 0, "probe with an incompatible coprocessor");
	auto start = std::chrono::steady_clock::now();
	int ret = agx_rtbuddy_boot(&r.rb);
	expect(ret == AGX_EINVAL, "incompatible version fails with EINVAL");
	expect(elapsed_ms(start) < 400, "version failure returns before the stage timeout");
	expect(r.asc.run_clear_count == 1, "cpu stopped after version failure");
	agx_rtbuddy_remove(&r.rb);
}

void test_stuck_mailbox()
{
	Rig r;

	r.asc.a2i_capacity = 1;
	r.asc.fw_ignore = true;
	expect(r.probe() == 0, "probe with a stuck mailbox");
	auto start = std::chrono::steady_clock::now();
	int ret = agx_rtbuddy_boot(&r.rb);
	long ms = elapsed_ms(start);
	expect(ret == -ETIMEDOUT, "stuck mailbox fails boot with ETIMEDOUT");
	expect(ms < 450, "send failure wakes the waiter before the stage timeout");
	agx_rtbuddy_remove(&r.rb);
	expect(r.asc.overflow == 0, "driver never overflowed the stuck mailbox");
}

void test_crash_after_boot()
{
	Rig r;

	expect(r.probe() == 0, "probe for the crash test");
	expect(agx_rtbuddy_boot(&r.rb) == 0, "boot for the crash test");
	r.asc.push_to_ap(AGX_RTKIT_EP_CRASHLOG,
			 static_cast<uint64_t>(AGX_RTKIT_CRASHLOG_CRASH)
				 << AGX_RTKIT_TYPE_SHIFT);
	bool seen = false;
	for (int i = 0; i < 2000 && !seen; i++) {
		seen = agx_rtkit_crashed(&r.rb.rtkit);
		if (!seen)
			usleep(1000);
	}
	expect(seen, "crash message after boot marks the coprocessor crashed");
	agx_rtbuddy_remove(&r.rb);
}

void test_gpu_messages()
{
	Rig r;

	expect(r.probe() == 0, "probe for the gpu message test");
	expect(agx_rtbuddy_send_initdata(&r.rb, 0x1000) == -ENODEV,
	       "initdata refused before boot");
	expect(agx_rtbuddy_doorbell(&r.rb, 1) == -ENODEV, "doorbell refused before boot");
	expect(agx_rtbuddy_boot(&r.rb) == 0, "boot for the gpu message test");

	expect(agx_rtbuddy_send_initdata(&r.rb, 0x123456789000ULL) == 0, "initdata sent");
	expect(agx_rtbuddy_doorbell(&r.rb, AGX_GPU_DOORBELL_KICK) == 0, "doorbell sent");
	expect(agx_rtbuddy_fwctl_doorbell(&r.rb) == 0, "firmware control doorbell sent");

	expect(agx_rtbuddy_kick(&r.rb, 4, false) == 0, "kick sent");
	expect(agx_rtbuddy_kick(&r.rb, 1, true) == 0, "alternate kick sent");
	expect(agx_rtbuddy_stop_firmware(&r.rb, 0x55) == 0, "stop message sent");

	bool got = r.wait_for([](FakeAsc &a) { return a.fw.gpu_msgs.size() >= 6; });
	expect(got, "coprocessor received all six gpu messages");

	r.with_fw([&](agxtest::FakeFirmware &fw) {
		if (fw.gpu_msgs.size() < 3)
			return 0;
		const Msg &a = fw.gpu_msgs[0];
		const Msg &b = fw.gpu_msgs[1];
		const Msg &c = fw.gpu_msgs[2];
		expect(a.ep == 0x20 && agx_gpu_msg_type(a.data) == AGX_GPU_MSG_INIT &&
			       agx_gpu_msg_initdata(a.data) == 0x23456789000ULL,
		       "initdata goes to endpoint 0x20 and is masked to 44 bits");
		expect(b.ep == 0x21 && agx_gpu_msg_type(b.data) == AGX_GPU_MSG_DOORBELL &&
			       agx_gpu_msg_channel(b.data) == 0x10,
		       "doorbell goes to endpoint 0x21 with channel 0x10");
		expect(c.ep == 0x21 && agx_gpu_msg_type(c.data) == AGX_GPU_MSG_FWCTL,
		       "firmware control doorbell goes to endpoint 0x21");
		expect(fw.gpu_msgs[3].ep == 0x21 && fw.gpu_msgs[3].data == 0x0083000000000010ULL,
		       "kick index 4 goes out as 0x0083000000000010 on endpoint 0x21");
		expect(fw.gpu_msgs[4].ep == 0x21 && fw.gpu_msgs[4].data == 0x0086000000000004ULL,
		       "alternate kick index 1 goes out as 0x0086000000000004");
		expect(fw.gpu_msgs[5].ep == 0x21 && fw.gpu_msgs[5].data == 0x0085000000000055ULL,
		       "stop message goes out on endpoint 0x21 with its counter");
		return 0;
	});

	for (int i = 0; i < 3; i++)
		r.asc.push_to_ap(AGX_RTKIT_EP_FIRMWARE,
				 static_cast<uint64_t>(AGX_GPU_MSG_EVENT)
					 << AGX_GPU_MSG_TYPE_SHIFT);
	r.asc.push_to_ap(AGX_RTKIT_EP_FIRMWARE, 0x00c2ULL << 48);
	r.asc.push_to_ap(AGX_RTKIT_EP_FIRMWARE, 0x7777ULL << 48);
	r.wait_for([](FakeAsc &a) { return a.i2a.empty(); });
	usleep(100000);
	agx_rtbuddy_remove(&r.rb);
	expect(r.rb.fw_events == 4, "four firmware events counted, including type 0xc2");
	expect(r.rb.app_unknown == 1, "one unrecognised firmware message counted");
}

void test_concurrent_senders()
{
	Rig r;

	r.asc.a2i_capacity = 4;
	expect(r.probe() == 0, "probe for the concurrency test");
	expect(agx_rtbuddy_boot(&r.rb) == 0, "boot for the concurrency test");

	const int threads = 4;
	const int per_thread = 200;
	std::vector<std::thread> pool;
	std::atomic<int> failures(0);

	for (int t = 0; t < threads; t++) {
		pool.emplace_back([&, t] {
			for (int i = 0; i < per_thread; i++)
				if (agx_rtbuddy_doorbell(&r.rb, static_cast<u32>(t)) != 0)
					failures++;
		});
	}
	for (std::thread &th : pool)
		th.join();

	bool got = r.wait_for(
		[&](FakeAsc &a) {
			return a.fw.gpu_msgs.size() >= static_cast<size_t>(threads * per_thread);
		},
		5000);
	expect(failures == 0, "no concurrent doorbell send failed");
	expect(got, "all concurrent doorbells reached the coprocessor");

	int bad = 0;
	int counts[4] = {0, 0, 0, 0};
	r.with_fw([&](agxtest::FakeFirmware &fw) {
		for (const Msg &m : fw.gpu_msgs) {
			if (m.ep != 0x21 || agx_gpu_msg_type(m.data) != AGX_GPU_MSG_DOORBELL ||
			    agx_gpu_msg_channel(m.data) > 3)
				bad++;
			else
				counts[agx_gpu_msg_channel(m.data)]++;
		}
		return 0;
	});
	expect(bad == 0, "every concurrent message arrived intact");
	expect(counts[0] == per_thread && counts[1] == per_thread &&
		       counts[2] == per_thread && counts[3] == per_thread,
	       "each sender delivered exactly its own messages");
	expect(r.asc.overflow == 0 && r.asc.order_violation == 0,
	       "mailbox lock kept concurrent sends ordered and within capacity");
	agx_rtbuddy_remove(&r.rb);
}

void test_repeated_boot_and_remove()
{
	int bad = 0;

	for (int i = 0; i < 25; i++) {
		Rig r;
		if (r.probe() != 0 || agx_rtbuddy_boot(&r.rb) != 0 ||
		    !agx_rtbuddy_started(&r.rb))
			bad++;
		agx_rtbuddy_remove(&r.rb);
		if (dma_outstanding() != 0 || r.asc.run_clear_count != 1)
			bad++;
	}
	expect(bad == 0, "25 boot and remove cycles with no leak or failure");
}

}

int main()
{
	g_verbose = std::getenv("AGX_HARNESS_VERBOSE") != nullptr;
	test_happy_path();
	test_outbox_gating_model();
	test_polling_when_full();
	test_silent_firmware();
	test_incompatible_version();
	test_stuck_mailbox();
	test_crash_after_boot();
	test_gpu_messages();
	test_concurrent_senders();
	test_repeated_boot_and_remove();
	return agxtest::finish();
}
