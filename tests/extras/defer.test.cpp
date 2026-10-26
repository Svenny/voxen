#include <extras/defer.hpp>

#include "../test_common.hpp"

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
		catch (int) {
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
		catch (int) {
			REQUIRE(flag == true);
		}
	}
}

namespace
{

// Runs a scope with `defer_fail` from a destructor, which is called while an exception is in flight
struct NormalExitDuringUnwinding {
	bool &fired;

	~NormalExitDuringUnwinding()
	{
		defer_fail { fired = true; };
	}
};

} // namespace

TEST_CASE("'defer_fail' ignores exceptions that were in flight before it", "[extras::defer]")
{
	bool fired = false;

	try {
		NormalExitDuringUnwinding guard { fired };
		throw 0;
	}
	catch (int) {
		REQUIRE(fired == false);
	}
}
