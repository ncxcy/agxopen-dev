#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>
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

	std::cout << std::left
		  << std::setw(20) << "timestampns"
		  << std::setw(12) << "endpoint"
		  << std::setw(12) << "flags"
		  << "data0" << std::endl;

	agxtrace::record rec{};
	size_t count = 0;

	while (in.read(reinterpret_cast<char *>(&rec), sizeof(rec))) {
		std::ostringstream ep, fl, d0;
		ep << "0x" << std::hex << rec.endpoint;
		fl << "0x" << std::hex << rec.flags;
		d0 << "0x" << std::hex << rec.data0;

		std::cout << std::left
			  << std::setw(20) << std::dec << rec.timestampns
			  << std::setw(12) << ep.str()
			  << std::setw(12) << fl.str()
			  << d0.str()
			  << std::endl;
		count++;
	}

	std::cout << std::endl << count << " records" << std::endl;

	return 0;
}
