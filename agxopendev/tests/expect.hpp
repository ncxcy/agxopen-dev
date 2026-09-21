#ifndef AGX_TEST_EXPECT_HPP
#define AGX_TEST_EXPECT_HPP

#include <iostream>
#include <string>

namespace agxtest {

inline int &failures()
{
	static int count = 0;
	return count;
}

inline void expect(bool condition, const std::string &description)
{
	if (condition) {
		std::cout << "pass  " << description << std::endl;
	} else {
		std::cout << "fail  " << description << std::endl;
		failures()++;
	}
}

inline int finish()
{
	std::cout << std::endl;
	if (failures() == 0) {
		std::cout << "all tests passed" << std::endl;
		return 0;
	}
	std::cout << failures() << " tests failed" << std::endl;
	return 1;
}

}

#endif
