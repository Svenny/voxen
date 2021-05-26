#include <extras/bitset.hpp>

#include <catch2/catch.hpp>

TEST_CASE("'bitset' free-list features work properly", "[extras::bitset]")
{
	extras::bitset<512> set;

	REQUIRE(set.first_zero() == 0);
	REQUIRE(set.occupy_zero() == 0);

	REQUIRE(set.first_zero() == 1);
	REQUIRE(set.occupy_zero() == 1);

	// Occupy all bits
	while (set.occupy_zero() < 511) {}

	REQUIRE(set.popcount() == 512);
	REQUIRE(set.first_zero() == SIZE_MAX);
	REQUIRE(set.occupy_zero() == SIZE_MAX);

	set.clear(511);
	set.clear(127);
	set.clear(16);
	set.clear(10);
	REQUIRE(set.popcount() == 508);
	REQUIRE(set.first_zero() == 10);

	REQUIRE(set.occupy_zero() == 10);
	REQUIRE(set.occupy_zero() == 16);
	REQUIRE(set.occupy_zero() == 127);
	REQUIRE(set.occupy_zero() == 511);
	REQUIRE(set.occupy_zero() == SIZE_MAX);
}

TEST_CASE("'bitset' basic functions work properly", "[extras::bitset]")
{
	SECTION("1-bit set")
	{
		extras::bitset<1> set;
		// Sanity checks
		REQUIRE(sizeof(set) <= sizeof(uint64_t));
		REQUIRE(set.popcount() == 0);

		set.set(0);
		REQUIRE(set.test(0) == true);
		REQUIRE(set.popcount() == 1);

		set.clear(0);
		REQUIRE(set.test(0) == false);
		REQUIRE(set.popcount() == 0);

		set.set();
		REQUIRE(set.test(0) == true);
		REQUIRE(set.popcount() == 1);
	}

	SECTION("64-bit set")
	{
		extras::bitset<64> set;
		// Sanity checks
		REQUIRE(sizeof(set) <= sizeof(uint64_t));
		REQUIRE(set.popcount() == 0);

		set.set(5);
		set.set(11);
		REQUIRE(set.test(5) == true);
		REQUIRE(set.test(10) == false);
		REQUIRE(set.popcount() == 2);

		set.set();
		REQUIRE(set.test(0) == true);
		REQUIRE(set.test(63) == true);
		REQUIRE(set.popcount() == 64);
	}

	SECTION("96-bit set")
	{
		extras::bitset<96> set;
		// Sanity checks
		REQUIRE(sizeof(set) <= 2 * sizeof(uint64_t));
		REQUIRE(set.popcount() == 0);

		set.set(63);
		set.set(64);
		REQUIRE(set.test(63) == true);
		REQUIRE(set.test(64) == true);
		REQUIRE(set.popcount() == 2);

		set.set(62);
		set.clear(65);
		set.clear(95);
		REQUIRE(set.test(64) == true);
		REQUIRE(set.popcount() == 3);

		set.clear(64);
		REQUIRE(set.test(64) == false);
		REQUIRE(set.popcount() == 2);

		set.clear();
		REQUIRE(set.popcount() == 0);

		set.set();
		REQUIRE(set.popcount() == 96);
	}
}
