#include <extras/function_ref.hpp>

#include <catch2/catch.hpp>

static int testFunction(int a, int b)
{
	return a + b;
}

TEST_CASE("'function_ref' can be created from various objects", "[extras::function_ref]")
{
	int x = 2;

	auto lambda1 = [](int a, int b) { return a + b; };
	auto lambda2 = [x](int a) { return x + a; };
	auto lambda3 = [&](int a) { return x += a; };

	extras::function_ref fn1(testFunction);
	extras::function_ref fn2(lambda1);
	extras::function_ref fn3(lambda2);
	extras::function_ref fn4(lambda3);

	REQUIRE(fn1(2, 3) == fn2(2, 3));
	REQUIRE(fn3(3) == fn2(2, 3));
	REQUIRE(fn4(1) == 3);
	REQUIRE(fn3(3) == 5);
	REQUIRE(fn4(1) == 4);
	REQUIRE(fn3(3) == 5);

	// Check `operator bool`
	extras::function_ref<void()> empty_ref;
	REQUIRE(!empty_ref);
	REQUIRE(fn4);
}

static void dummy() noexcept
{}

TEST_CASE("'function_ref' handles noexcept properly", "[extras::function_ref]")
{
	auto lambda1 = []() {};
	auto lambda2 = []() noexcept {};

	extras::function_ref fn1(lambda1);
	extras::function_ref fn2(lambda2);
	extras::function_ref fn3(dummy);
	REQUIRE(!noexcept(fn1()));
	REQUIRE(noexcept(fn2()));
	REQUIRE(noexcept(fn3()));

	extras::function_ref<void()> fn5(lambda2);
	extras::function_ref<void()> fn6(fn2);
	fn3();
	fn5();
}
