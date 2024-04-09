#include <extras/defer.hpp>

#include <catch2/catch.hpp>

TEST_CASE("'defer' works properly", "[extras::defer]")
{
	SECTION("exiting scope normally")
	{
		bool flag1 = false;
		bool flag2 = false;

		{
			defer {
				// Ensure defers run in reverse order of declaration
				REQUIRE(flag2 == true);
				flag1 = true;
			};

			defer { flag2 = true; };
		}

		REQUIRE(flag1 == true);
		REQUIRE(flag2 == true);
	}

	SECTION("exiting scope via exception")
	{
		bool flag = false;

		try {
			defer { flag = true; };

			throw 0;
		}
		catch (int ex) {
			REQUIRE(flag == true);
		}
	}
}

TEST_CASE("'defer_fail' works properly", "[extras::defer]")
{
	bool flag = false;

	SECTION("exiting scope normally")
	{
		{
			defer_fail { flag = true; };
		}

		REQUIRE(flag == false);
	}

	SECTION("exiting scope via exception")
	{
		try {
			defer_fail { flag = true; };

			throw 0;
		}
		catch (int ex) {
			REQUIRE(flag == true);
		}
	}
}
