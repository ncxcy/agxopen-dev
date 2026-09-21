#ifndef AGX_TEST_FAKEFW_HPP
#define AGX_TEST_FAKEFW_HPP

#include <cstdint>
#include <deque>
#include <set>
#include <vector>

#include "agx/power.h"
#include "agx/rtkit.h"

namespace agxtest {

struct Msg {
	uint32_t ep;
	uint64_t data;
};

uint64_t mgmt(uint32_t type, uint64_t payload)
{
	return (static_cast<uint64_t>(type) << AGX_RTKIT_TYPE_SHIFT) | payload;
}

uint32_t mgmt_type(uint64_t msg)
{
	return static_cast<uint32_t>((msg >> AGX_RTKIT_TYPE_SHIFT) & 0xff);
}

struct FakeFirmware {
	uint32_t min_ver = 11;
	uint32_t max_ver = 12;
	std::deque<Msg> to_ap;
	std::vector<Msg> from_ap;
	std::vector<uint64_t> hello_replies;
	std::vector<uint64_t> epmap_replies;
	std::vector<uint32_t> started;
	std::vector<Msg> buffer_replies;
	std::vector<Msg> gpu_msgs;
	std::set<uint32_t> known_eps;
	bool pending_iop = false;
	uint32_t iop_state_requested = 0;
	bool epmap_finished = false;

	void power_on()
	{
		to_ap.push_back({0, mgmt(AGX_RTKIT_MGMT_HELLO,
					 (static_cast<uint64_t>(max_ver) << 16) | min_ver)});
	}

	void push_epmap(uint32_t base, uint32_t bitmap, bool last)
	{
		uint64_t m = mgmt(AGX_RTKIT_MGMT_EPMAP,
				  (static_cast<uint64_t>(base) << 32) | bitmap);
		if (last)
			m |= AGX_RTKIT_EPMAP_LAST;
		to_ap.push_back({0, m});
	}

	void handle(uint32_t ep, uint64_t msg)
	{
		from_ap.push_back({ep, msg});
		if (ep == 0)
			handle_mgmt(msg);
		else if (ep >= AGX_RTKIT_EP_APP_START)
			gpu_msgs.push_back({ep, msg});
		else
			buffer_replies.push_back({ep, msg});
	}

	void handle_mgmt(uint64_t msg)
	{
		switch (mgmt_type(msg)) {
		case AGX_RTKIT_MGMT_HELLO_REPLY:
			hello_replies.push_back(msg);
			push_epmap(0, 0x11f, false);
			break;
		case AGX_RTKIT_MGMT_EPMAP_REPLY:
			epmap_replies.push_back(msg);
			if (msg & AGX_RTKIT_EPMAP_MORE) {
				push_epmap(1, 0x3, true);
			} else if (msg & AGX_RTKIT_EPMAP_LAST) {
				epmap_finished = true;
				if (pending_iop)
					to_ap.push_back({0, mgmt(AGX_RTKIT_MGMT_IOP_PWR_ACK, 0x20)});
			}
			break;
		case AGX_RTKIT_MGMT_IOP_PWR: {
			uint32_t state = static_cast<uint32_t>(msg & 0xffff);
			uint32_t ack = state == AGX_PWR_INIT ? AGX_PWR_ON : state;
			iop_state_requested = state;
			if (state == AGX_PWR_INIT) {
				pending_iop = true;
				if (epmap_finished)
					to_ap.push_back({0, mgmt(AGX_RTKIT_MGMT_IOP_PWR_ACK, ack)});
			} else {
				to_ap.push_back({0, mgmt(AGX_RTKIT_MGMT_IOP_PWR_ACK, ack)});
			}
			break;
		}
		case AGX_RTKIT_MGMT_AP_PWR:
			to_ap.push_back({0, mgmt(AGX_RTKIT_MGMT_AP_PWR_ACK,
						 msg & 0xffff)});
			break;
		case AGX_RTKIT_MGMT_STARTEP: {
			uint32_t ep = static_cast<uint32_t>((msg >> 32) & 0xff);
			started.push_back(ep);
			if (!(msg & AGX_RTKIT_STARTEP_FLAG))
				break;
			if (ep == AGX_RTKIT_EP_CRASHLOG ||
			    ep == AGX_RTKIT_EP_SYSLOG ||
			    ep == AGX_RTKIT_EP_IOREPORT) {
				uint64_t req = (static_cast<uint64_t>(1) << 52) |
					       (static_cast<uint64_t>(2) << 44);
				to_ap.push_back({ep, req});
			} else if (ep == AGX_RTKIT_EP_OSLOG) {
				uint64_t req = (static_cast<uint64_t>(1) << 56) |
					       (static_cast<uint64_t>(0x2000) << 36);
				to_ap.push_back({ep, req});
			}
			break;
		}
		default:
			break;
		}
	}
};

}

#endif
