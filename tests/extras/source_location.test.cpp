#include <extras/source_location.hpp>

#include "../test_common.hpp"

TEST_CASE("'source_location' works properly", "[extras::source_location]")
{
	auto loc = extras::source_location::current();

#ifndef _WIN32
	CHECK(std::string(loc.file_name()) == "tests/extras/source_location.test.cpp");
#else
	CHECK(std::string(loc.file_name()) == "tests\\extras\\source_location.test.cpp");
#endif

	CHECK(loc.line() == 7);
}
