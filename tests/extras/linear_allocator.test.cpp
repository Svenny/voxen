#include <extras/linear_allocator.hpp>

#include "../test_common.hpp"

#include <cstdint>
#include <limits>
#include <vector>

namespace extras
{

namespace
{

template<typename T>
class TestAllocator final : public extras::linear_allocator<TestAllocator<T>, T, 32> {
public:
	using Base = extras::linear_allocator<TestAllocator<T>, T, 32>;

	TestAllocator() : Base(std::numeric_limits<T>::max() / 10) {}

	void setAboutToFree() noexcept { m_about_to_free = true; }

	static void on_allocator_freed(Base &base)
	{
		auto &me = static_cast<TestAllocator &>(base);
		REQUIRE(me.is_free());
		REQUIRE(me.m_about_to_free);
		me.m_about_to_free = false;
	}

private:
	bool m_about_to_free = false;
};

class TestAllocator2 final : public extras::linear_allocator<TestAllocator2, uint32_t, 4> {
public:
	using Base = extras::linear_allocator<TestAllocator2, uint32_t, 4>;

	TestAllocator2() : Base(1024) {}

	void setAboutToDestroy() noexcept { m_about_to_destroy = true; }

	static void on_allocator_freed(Base &base)
	{
		auto &me = static_cast<TestAllocator2 &>(base);
		REQUIRE(!me.m_about_to_destroy);
	}

private:
	bool m_about_to_destroy = false;
};

using TestAllocator32 = TestAllocator<uint32_t>;
using TestAllocator64 = TestAllocator<uint64_t>;

TEST_CASE("Basic test of 'linear_allocator'", "[extras::linear_allocator]")
{
	TestAllocator32 alloc;

	auto range1 = alloc.allocate(256, 32);
	REQUIRE(range1.has_value());
	REQUIRE(range1->first == 0);
	REQUIRE(range1->second == 256);

	auto range2 = alloc.allocate(512, 32);
	REQUIRE(range2.has_value());
	REQUIRE(range2->first == 256);
	REQUIRE(range2->second == 768);

	alloc.free(*range1);

	auto range3 = alloc.allocate(128, 32);
	REQUIRE(range3.has_value());
	REQUIRE(range3->first == 0);
	REQUIRE(range3->second == 128);

	auto range4 = alloc.grow(*range3, 64);
	REQUIRE(range4.has_value());
	REQUIRE(range4->first == 0);
	REQUIRE(range4->second == 192);

	auto range5 = alloc.grow(*range4, 500);
	REQUIRE(!range5.has_value());

	alloc.free(*range4);

	alloc.setAboutToFree();
	alloc.free(*range2);
	REQUIRE(alloc.is_free());
}

TEST_CASE("'linear_allocator' works properly with 64-bit addresses", "[extras::linear_allocator]")
{
	TestAllocator64 alloc;

	constexpr uint64_t SZ = 1'000'000'000'000'000;

	auto range1 = alloc.allocate(SZ, 4096);
	REQUIRE(range1.has_value());
	REQUIRE(range1->first == 0);
	REQUIRE(range1->second >= SZ);
	REQUIRE(range1->second % 4096 == 0);

	auto range2 = alloc.allocate(SZ, 4096);
	REQUIRE(range2.has_value());
	REQUIRE(range2->first == range1->second);
	REQUIRE(range2->second - range2->first >= SZ);

	alloc.setAboutToFree();
}

TEST_CASE("'linear_allocator' doesn't call free callback from destructor", "[extras::linear_allocator]")
{
	{
		// Not allocated anything
		TestAllocator2 alloc;
		alloc.setAboutToDestroy();
	}
	{
		// Allocated but then freed
		TestAllocator2 alloc;

		auto range = alloc.allocate(40, 16);
		alloc.free(*range);

		alloc.setAboutToDestroy();
	}
	{
		// Allocated but not freed
		TestAllocator2 alloc;

		auto range = alloc.allocate(40, 16);
		(void) range;

		alloc.setAboutToDestroy();
	}
}

} // namespace

namespace
{

// Starts with a tiny free range reservation and exposes its capacity
class TestAllocator3 final : public extras::linear_allocator<TestAllocator3, uint32_t, 64> {
public:
	using Base = extras::linear_allocator<TestAllocator3, uint32_t, 64>;

	TestAllocator3() : Base(1 << 20, 2) {}

	size_t freeRangesCapacity() const noexcept { return m_free_ranges.capacity(); }

	static void on_allocator_freed(Base &) noexcept {}
};

} // namespace

TEST_CASE("'linear_allocator' doesn't reallocate in free()", "[extras::linear_allocator]")
{
	TestAllocator3 alloc;

	// Sequential allocations from the front keep a single free range
	std::vector<TestAllocator3::range_type> ranges;
	for (int i = 0; i < 64; i++) {
		auto range = alloc.allocate(64, 64);
		REQUIRE(range.has_value());
		ranges.push_back(*range);
	}

	// Freeing every other allocation creates a new free range each time
	const size_t capacity = alloc.freeRangesCapacity();
	for (size_t i = 0; i < ranges.size(); i += 2) {
		alloc.free(ranges[i]);
		CHECK(alloc.freeRangesCapacity() == capacity);
	}

	for (size_t i = 1; i < ranges.size(); i += 2) {
		alloc.free(ranges[i]);
	}

	CHECK(alloc.is_free());
}

} // namespace extras
