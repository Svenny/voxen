#include <extras/dyn_array.hpp>

#include "../test_common.hpp"

#include <array>
#include <unordered_set>

namespace
{

template<typename T>
struct CountingAllocator {
	using value_type = T;

	CountingAllocator(size_t &count) noexcept : m_count(&count) {}
	CountingAllocator(CountingAllocator &&) = default;
	CountingAllocator(const CountingAllocator &) = default;
	CountingAllocator &operator=(CountingAllocator &&) = default;
	CountingAllocator &operator=(const CountingAllocator &) = default;
	~CountingAllocator() = default;

	T *allocate(size_t n)
	{
		*m_count += n;
		return static_cast<T *>(::operator new(sizeof(T) * n));
	}

	void deallocate(T *p, size_t n) noexcept
	{
		*m_count -= n;
		::operator delete(p);
	}

	bool operator==(const CountingAllocator &other) const noexcept { return m_count == other.m_count; }

	size_t *m_count;
};

struct Mine {
	Mine(int &ticks) : m_ticks(ticks) {}
	Mine(Mine &&) = delete;
	Mine &operator=(Mine &&) = delete;

	Mine(const Mine &other) : m_ticks(other.m_ticks)
	{
		m_ticks--;
		if (m_ticks == 0) {
			throw std::runtime_error("boom");
		}
	}

	Mine &operator=(const Mine &)
	{
		m_ticks--;
		if (m_ticks == 0) {
			throw std::runtime_error("boom");
		}
		return *this;
	}

	int &m_ticks;
};

struct TrackedObject;

using TrackSet = std::unordered_set<TrackedObject *>;

struct TrackedObject {
	TrackedObject(TrackSet &set, bool explosive = false) : m_set(&set), m_explosive(explosive)
	{
		auto inserted = set.emplace(this);
		CHECK(inserted.second == true);
	}

	TrackedObject(TrackedObject &&other) noexcept : m_set(other.m_set)
	{
		auto inserted = m_set->emplace(this);
		CHECK(inserted.second == true);

		std::swap(m_explosive, other.m_explosive);
	}

	TrackedObject(const TrackedObject &other)
	{
		if (other.m_explosive) {
			throw std::runtime_error("boom");
		}

		m_set = other.m_set;
		m_explosive = other.m_explosive;

		auto inserted = m_set->emplace(this);
		CHECK(inserted.second == true);
	}

	TrackedObject &operator=(TrackedObject &&other) noexcept
	{
		std::swap(m_explosive, other.m_explosive);
		return *this;
	}

	TrackedObject &operator=(const TrackedObject &other)
	{
		if (other.m_explosive) {
			throw std::runtime_error("boom");
		}

		return *this;
	}

	~TrackedObject() noexcept
	{
		if (m_set) {
			CHECK(m_set->contains(this));
			m_set->erase(this);
		}
	}

	TrackSet *m_set = nullptr;
	bool m_explosive = false;
};

} // namespace

TEST_CASE("'dyn_array' handles empty arrays properly", "[extras::dyn_array]")
{
	extras::dyn_array<int> empty;

	// Nothing was allocated
	CHECK(empty.data() == nullptr);

	// Size is consistently zero
	CHECK(empty.empty());
	CHECK(empty.size() == 0);
	CHECK(empty.size_bytes() == 0);

	// Span conversions
	CHECK(empty.as_bytes().empty());
	std::span<int> span_empty = empty;
	CHECK(span_empty.empty());
}

TEST_CASE("'dyn_array' counts sizes properly", "[extras::dyn_array]")
{
	extras::dyn_array<int32_t> i32(15);
	CHECK(i32.size() == 15);
	CHECK(i32.size_bytes() == 15 * 4);
	CHECK(i32.as_bytes().size() == 15 * 4);

	extras::dyn_array<int16_t> i16(27);
	CHECK(i16.size_bytes() == 27 * 2);
	CHECK(i16.as_writable_bytes().size() == 27 * 2);

	extras::dyn_array<int8_t> i8(13);
	CHECK(i8.size_bytes() == 13);
	CHECK(i8.as_bytes().size() == 13);

	extras::dyn_array<int64_t> i64(11);
	CHECK(i64.size_bytes() == 11 * 8);

	using S17 = std::array<int8_t, 17>;
	static_assert(sizeof(S17) == 17);

	extras::dyn_array<S17> s17(33);
	CHECK(s17.size() == 33);
	CHECK(s17.size_bytes() == 33 * 17);
	CHECK(s17.as_bytes().size() == 33 * 17);
}

TEST_CASE("'dyn_array' does not leak memory", "[extras::dyn_array]")
{
	size_t count = 0;

	SECTION("non-throwing operations")
	{
		CountingAllocator<int32_t> alloc(count);
		using Array = extras::dyn_array<int32_t, CountingAllocator<int32_t>>;

		Array arr(0, alloc);
		CHECK(count == 0);

		Array arr2(5, alloc);
		CHECK(count == 5);

		{
			Array arr3(5, alloc);
			CHECK(count == 10);

			Array arr4(std::move(arr3));
			CHECK(count == 10);
		}
		CHECK(count == 5);

		arr = arr2;
		CHECK(count == 10);

		arr2 = Array(10, alloc);
		CHECK(count == 15);

		arr = Array(0, alloc);
		CHECK(count == 10);

		arr2 = arr;
		CHECK(count == 0);
	}

	SECTION("throwing object generation")
	{
		CountingAllocator<int32_t> alloc(count);

		auto go = [&] {
			extras::dyn_array<int32_t, CountingAllocator<int32_t>> arr(
				10,
				[](void *place, size_t index) {
					if (index == 8) {
						throw std::runtime_error("boom");
					}
					*static_cast<int32_t *>(place) = static_cast<int32_t>(index);
				},
				alloc);
		};

		CHECK_THROWS_WITH(go(), "boom");
		CHECK(count == 0);
	}

	CountingAllocator<Mine> alloc(count);
	using Array = extras::dyn_array<Mine, CountingAllocator<Mine>>;

	SECTION("throwing object copy construction")
	{
		int ticks = 5;
		Mine master_copy(ticks);

		auto go = [&] { Array mines(10, master_copy, alloc); };
		CHECK_THROWS_WITH(go(), "boom");
		CHECK(count == 0);
	}

	SECTION("throwing object copy assignment")
	{
		int ticks = 25;
		Mine master_copy(ticks);

		Array mines(alloc);
		CHECK_NOTHROW(mines = Array(10, master_copy, alloc));

		Array mines2(alloc);
		CHECK_NOTHROW(mines2 = Array(10, master_copy, alloc));

		// `mines` and `mines2` have equal size and allocator,
		// this will trigger no-reallocation `std::copy`
		CHECK_THROWS_WITH(mines2 = mines, "boom");

		CHECK_NOTHROW(mines = Array(alloc));
		CHECK_NOTHROW(mines2 = Array(alloc));
		CHECK(count == 0);
	}
}

TEST_CASE("'dyn_array' properly manages object lifetimes", "[extras::dyn_array]")
{
	TrackSet ts;
	using Array = extras::dyn_array<TrackedObject>;

	SECTION("throwing object generation")
	{
		auto go = [&] {
			Array arr(10, [&](void *place, size_t index) {
				if (index == 8) {
					throw std::runtime_error("boom");
				}

				new (place) TrackedObject(ts);
				CHECK(ts.size() == index + 1);
			});
		};

		CHECK_THROWS_WITH(go(), "boom");
		CHECK(ts.empty());
	}

	SECTION("throwing object copy construction")
	{
		TrackedObject master_copy(ts, true);

		auto go = [&] { Array arr(10, master_copy); };
		CHECK_THROWS_WITH(go(), "boom");
		CHECK(ts.size() == 1);
		CHECK(*ts.begin() == &master_copy);
	}

	SECTION("throwing object copy assignment")
	{
		TrackedObject master_copy(ts);

		Array arr(10, master_copy);
		Array arr2(10, master_copy);
		CHECK(ts.size() == 21);

		arr2[5].m_explosive = true;
		CHECK_THROWS_WITH(arr = arr2, "boom");
		CHECK(ts.size() == 21);
	}
}
