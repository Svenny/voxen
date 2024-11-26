#include <voxen/common/private_object_pool.hpp>

#include "../../voxen_test_common.hpp"

namespace voxen
{

TEST_CASE("'PrivateObjectPool' basic test case", "[voxen::private_object_pool]")
{
	using Pool = PrivateObjectPool<uint64_t, 2048>;
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

} // namespace voxen
