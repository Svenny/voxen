#include <extras/refcnt_ptr.hpp>

#include <catch2/catch.hpp>

static void managerFunction(int *value, extras::refcnt_ptr_action action) noexcept
{
	if (action == extras::refcnt_ptr_action::acquire_ref) {
		(*value)++;
	} else if (action == extras::refcnt_ptr_action::release_ref){
		(*value)--;
	} else {
		REQUIRE(false);
	}
}

TEST_CASE("'refcnt_ptr' calls manager function correctly", "[extras::refcnt_ptr]")
{
	int ref_count = 1;

	extras::refcnt_ptr ptr1(&ref_count, extras::function_ref(managerFunction));
	REQUIRE(ref_count == 1);

	{
		auto ptr2 = ptr1;
		REQUIRE(ref_count == 2);
	}
	REQUIRE(ref_count == 1);

	{
		auto ptr2 = ptr1;
		auto ptr3 = ptr1;
		REQUIRE(ref_count == 3);
		auto ptr4 = ptr2;
		REQUIRE(ref_count == 4);
	}
	REQUIRE(ref_count == 1);

	{
		auto ptr2 = ptr1;
		auto ptr3 = std::move(ptr2);
		REQUIRE(ref_count == 2);

		std::swap(ptr1, ptr3);
		REQUIRE(ref_count == 2);
	}
	REQUIRE(ref_count == 1);

	auto ptr2(ptr1);
	auto ptr3(std::move(ptr1));
	REQUIRE(ref_count == 2);

	ptr3 = extras::refcnt_ptr<int>();
	REQUIRE(ref_count == 1);

	ptr2.reset();
	REQUIRE(ref_count == 0);
}
