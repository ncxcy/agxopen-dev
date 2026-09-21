#ifndef agxtracehpp
#define agxtracehpp

#include <array>
#include <cstdint>

namespace agxtrace {

constexpr std::array<char, 8> magic = {'a', 'g', 'x', 't', 'r', 'c', '1', '\0'};

struct record {
	uint64_t timestampns;
	uint32_t endpoint;
	uint32_t flags;
	uint64_t data0;
};

constexpr uint32_t endpointmanagement = 0x00;
constexpr uint32_t endpointsystemlimit = 0x20;
constexpr uint32_t endpointfirmware = 0x20;
constexpr uint32_t endpointdoorbell = 0x21;

}

#endif
