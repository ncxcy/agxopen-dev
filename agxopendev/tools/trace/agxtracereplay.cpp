#include <fstream>
#include <iostream>
#include <array>
#include "agxtrace.hpp"

static bool readheader(std::ifstream &in)
{
	std::array<char, 8> header{};

	in.read(header.data(), header.size());
	if (!in)
		return false;

	return header == agxtrace::magic;
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		std::cerr << "usage " << argv[0] << " trace file path" << std::endl;
		return 1;
	}

	std::ifstream in(argv[1], std::ios::binary);
	if (!in) {
		std::cerr << "could not open trace file" << std::endl;
		return 1;
	}

	if (!readheader(in)) {
		std::cerr << "trace file has an unexpected header" << std::endl;
		return 1;
	}

	agxtrace::record rec{};
	agxtrace::record previous{};
	bool haveprevious = false;
	size_t total = 0;
	size_t anomalies = 0;
	size_t managementcount = 0;
	size_t systemcount = 0;
	size_t doorbellcount = 0;
	size_t firmwarecount = 0;
	size_t unknowncount = 0;

	while (in.read(reinterpret_cast<char *>(&rec), sizeof(rec))) {
		total++;

		if (haveprevious && rec.timestampns < previous.timestampns) {
			std::cerr << "anomaly at record " << total
				  << " timestamp went backwards" << std::endl;
			anomalies++;
		}

		if (rec.endpoint == agxtrace::endpointmanagement) {
			managementcount++;
		} else if (rec.endpoint < agxtrace::endpointsystemlimit) {
			systemcount++;
		} else if (rec.endpoint == agxtrace::endpointdoorbell) {
			doorbellcount++;
		} else if (rec.endpoint == agxtrace::endpointfirmware) {
			firmwarecount++;
		} else {
			std::cerr << "anomaly at record " << total
				  << " endpoint 0x" << std::hex << rec.endpoint
				  << std::dec << " is not a known endpoint" << std::endl;
			unknowncount++;
			anomalies++;
		}

		previous = rec;
		haveprevious = true;
	}

	std::cout << "replayed " << total << " records" << std::endl;
	std::cout << "management endpoint messages " << managementcount << std::endl;
	std::cout << "system endpoint messages " << systemcount << std::endl;
	std::cout << "doorbell endpoint messages " << doorbellcount << std::endl;
	std::cout << "firmware endpoint messages " << firmwarecount << std::endl;
	std::cout << "unknown endpoint messages " << unknowncount << std::endl;
	std::cout << "anomalies " << anomalies << std::endl;

	return anomalies == 0 ? 0 : 1;
}
