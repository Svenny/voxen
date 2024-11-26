#include <voxen/common/shared_object_pool.hpp>

#include "../../voxen_test_common.hpp"

#include <thread>

namespace voxen
{

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

TEST_CASE("'SharedObjectPool' the most basic test case", "[voxen::shared_object_pool]")
{
	SharedObjectPool<ValueChecker, 4> pool;

	int value = 0;
	auto ptr1 = pool.allocate(&value, 0);
	auto ptr2 = pool.allocate(&value, 1);
	auto ptr3 = pool.allocate(&value, 2);

	ptr1.reset(); // `value` must be 0 here

	auto ptr4 = ptr2;
	ptr2.reset();

	value = 1;
	ptr4.reset(); // `value` must be 1 here

	auto ptr5 = std::move(ptr3);
	value = 2;
	// `value` must be 2 here
}

TEST_CASE("'SharedObjectPool' basic test case", "[voxen::shared_object_pool]")
{
	using Pool = SharedObjectPool<uint64_t, 2048>;
	Pool pool;

	std::mt19937_64 rng(0xDEADBEEF);

	// Store values and pointers to check that pool does not clobber the memory
	std::vector<std::pair<uint64_t, Pool::Ptr>> objects(15'000);
	for (auto &[key, ptr] : objects) {
		key = rng();
		ptr = pool.allocate(key);
	}

	size_t errors = 0;

	// Free half of the objects to test mixed inserts/frees later
	for (size_t i = 0; i < objects.size() / 2; i++) {
		errors += (objects[i].first != *objects[i].second);
		objects[i].second.reset();
	}

	REQUIRE(errors == 0);

	for (int round = 0; round < 5; round++) {
		// Shuffle to get a random order of inserts/frees every time
		std::shuffle(objects.begin(), objects.end(), rng);

		for (size_t i = 0; i < objects.size(); i++) {
			if (!objects[i].second) {
				objects[i].second = pool.allocate(objects[i].first);
			} else {
				errors += (objects[i].first != *objects[i].second);
				objects[i].second.reset();
			}
		}

		REQUIRE(errors == 0);
	}
}

TEST_CASE("'SharedObjectPool' multithreaded deallocation", "[voxen::shared_object_pool]")
{
	using Pool = SharedObjectPool<uint64_t, 2048>;
	Pool pool;

	std::mt19937_64 rng(0xDEADBEEF);

	// Store values and pointers to check that pool does not clobber the memory
	std::vector<std::pair<uint64_t, Pool::Ptr>> objects;

	for (size_t i = 0; i < 15'000; i++) {
		uint64_t key = rng();
		objects.emplace_back(key, pool.allocate(key));
		// Copy a few times to make several references to the same object
		for (int j = 0; j < 5; j++) {
			objects.emplace_back(objects.back());
		}
	}

	std::shuffle(objects.begin(), objects.end(), rng);

	std::atomic_size_t errors = 0;

	// Launch threads to release all these references
	constexpr size_t THREADS = 4;
	std::thread threads[THREADS];

	for (size_t i = 0; i < THREADS; i++) {
		size_t begin = i * (objects.size() / THREADS);
		size_t end = i + 1 == THREADS ? objects.size() : begin + (objects.size() / THREADS);

		threads[i] = std::thread([&objects, &errors, begin, end]() {
			for (size_t j = begin; j < end; j++) {
				if (objects[j].first != *objects[j].second) {
					errors.fetch_add(1);
				}
				objects[j].second.reset();
			}
		});
	}

	for (size_t i = 0; i < THREADS; i++) {
		threads[i].join();
	}

	REQUIRE(errors.load() == 0);
}

} // namespace voxen
