#include <extras/source_location.hpp>

#include "../test_common.hpp"

TEST_CASE("'source_location' works properly", "[extras::source_location]")
{
	auto loc = extras::source_location::current();

	REQUIRE(std::string(loc.file_name()) == "tests/extras/source_location.test.cpp");
	REQUIRE(loc.line() == 7);
}
