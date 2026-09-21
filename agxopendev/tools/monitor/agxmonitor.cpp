#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

static std::string readfile(const std::string &path)
{
	std::ifstream in(path);
	std::string line;

	if (!in)
		return "unavailable";

	std::getline(in, line);

	return line;
}

int main()
{
	const std::string root = "/sys/kernel/debug/agxrtbuddy";

	while (true) {
		std::cout << "\033[2J\033[H";
		std::cout << "agx rtbuddy live monitor" << std::endl;
		std::cout << std::endl;
		std::cout << "power state    " << readfile(root + "/power") << std::endl;
		std::cout << "drain count    " << readfile(root + "/draincount") << std::endl;
		std::cout << std::endl;

		std::ifstream endpoints(root + "/endpoints");
		std::string line;

		while (std::getline(endpoints, line))
			std::cout << line << std::endl;

		std::cout << std::endl;

		std::ifstream rtkit(root + "/rtkit");

		while (std::getline(rtkit, line))
			std::cout << line << std::endl;

		std::cout << std::endl << "refreshing every second, press control c to quit" << std::endl;

		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	return 0;
}
