#include <extras/fixed_pool.hpp>

#include "../test_common.hpp"

namespace
{

class ValueChecker final {
public:
	ValueChecker(int *value_ptr, int value) noexcept : m_value_ptr(value_ptr), m_required_value(value) {}

	ValueChecker(ValueChecker &&) = delete;
	ValueChecker(const ValueChecker &) = delete;
	ValueChecker &operator=(ValueChecker &&) = delete;
	ValueChecker &operator=(const ValueChecker &) = delete;

	~ValueChecker() noexcept { REQUIRE(*m_value_ptr == m_required_value); }

private:
	int *m_value_ptr = nullptr;
	const int m_required_value;
};

} // namespace

TEST_CASE("The most basic test of 'fixed_pool'", "[extras::fixed_pool]")
{
	extras::fixed_pool<ValueChecker, 4> pool;

	int value = 0;
	auto ptr1 = pool.allocate(&value, 0);
	auto ptr2 = pool.allocate(&value, 1);
	auto ptr3 = pool.allocate(&value, 2);

	REQUIRE(pool.free_space() == 1);

	ptr1.reset(); // `value` must be 0 here

	REQUIRE(pool.free_space() == 2);

	auto ptr4 = ptr2;
	ptr2.reset();
	REQUIRE(pool.free_space() == 2);

	value = 1;
	ptr4.reset(); // `value` must be 1 here
	REQUIRE(pool.free_space() == 3);

	auto ptr5 = std::move(ptr3);
	value = 2;
	// `value` must be 2 here
}

namespace
{

class ReusableObject final {
public:
	~ReusableObject() noexcept { REQUIRE(m_value == 0); }

	void afterAllocated() { REQUIRE(m_value == 0); }

	void add(int value) { m_value += value; }

	void clear() noexcept { m_value = 0; }

private:
	int m_value = 0;
};

} // namespace

TEST_CASE("The most basic test of 'reusable_fixed_pool'", "[extras::fixed_pool]")
{
	extras::reusable_fixed_pool<ReusableObject, 3> pool;

	auto ptr1 = pool.allocate();
	auto ptr2 = pool.allocate();
	auto ptr3 = pool.allocate();
	REQUIRE(ptr3);
	auto ptr4 = pool.allocate();
	REQUIRE(!ptr4);

	ptr1->afterAllocated();
	ptr1->add(5);
	ptr1 = extras::refcnt_ptr<ReusableObject>();

	ptr2->afterAllocated();
	ptr2->add(2);
	ptr3->afterAllocated();
	std::swap(ptr2, ptr3);

	ptr2->add(5);
	ptr3->add(3);
}
