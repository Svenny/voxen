#include <extras/source_location.hpp>

#include <catch2/catch.hpp>

TEST_CASE("'source_location' works properly", "[extras::source_location]")
{
	auto loc = extras::source_location::current();

	REQUIRE(std::string(loc.file_name()) == "tests/extras/source_location.cpp");
	REQUIRE(loc.line() == 7);
}
