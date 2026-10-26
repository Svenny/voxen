#include "../test_common.hpp"

#include <source_location>
#include <string>

// Checks the build setup rather than `std`: file names are relative to the source root
TEST_CASE("'source_location' has relative file names", "[extras::source_location]")
{
	auto loc = std::source_location::current();

#ifndef _WIN32
	CHECK(std::string(loc.file_name()) == "tests/extras/source_location.test.cpp");
#else
	CHECK(std::string(loc.file_name()) == "tests\\extras\\source_location.test.cpp");
#endif

	CHECK(loc.line() == 9);
}
