#include <voxen/land/compressed_chunk_storage.hpp>

#include <voxen/land/land_utils.hpp>

#include "../../voxen_test_common.hpp"

namespace voxen::land
{

namespace
{

constexpr uint32_t N = Consts::CHUNK_SIZE_BLOCKS;

template<typename T>
void test(uint32_t seed)
{
	auto source = std::make_unique<CubeArray<T, N>>();
	auto dest = std::make_unique<CubeArray<T, N>>();

	std::mt19937 rng(seed);

	// Test a few times with different random values
	for (int i = 0; i < 10; i++) {
		// Fill `source` with random values
		for (auto &item : *source) {
			item = static_cast<T>(rng());
		}

		CompressedChunkStorage<T> storage(source->cview());

		// Check single value loads from compressed storage
		Utils::forYXZ<N>([&](uint32_t x, uint32_t y, uint32_t z) {
			T expected = source->load(x, y, z);
			T actual = storage.load(x, y, z);

			// Don't spam assertions count, and also log the failure location
			if (expected != actual) {
				INFO("Compressed storage load check failed");
				INFO("Failure point: " << x << " " << y << " " << z);
				CHECK(expected == actual);
			}
		});

		// Check compression-decompression round-trip
		storage.expand(dest->view());

		// Don't spam assertions count, and also log the failure location
		if (*source != *dest) {
			INFO("Compression round-trip check failed");
			Utils::forYXZ<N>([&](uint32_t x, uint32_t y, uint32_t z) {
				T expected = source->load(x, y, z);
				T actual = dest->load(x, y, z);

				if (expected != actual) {
					INFO("Failure point: " << x << " " << y << " " << z);
					CHECK(expected == actual);
				}
			});
		}
	}
}

} // namespace

TEST_CASE("'CompressedChunkStorage<uint8_t>' random values round-trip", "[voxen::land::compressed_chunk_storage]")
{
	test<uint8_t>(0xDEADBEEF + 8);
}

TEST_CASE("'CompressedChunkStorage<uint16_t>' random values round-trip", "[voxen::land::compressed_chunk_storage]")
{
	test<uint16_t>(0xDEADBEEF + 16);
}

TEST_CASE("'CompressedChunkStorage<uint32_t>' random values round-trip", "[voxen::land::compressed_chunk_storage]")
{
	test<uint32_t>(0xDEADBEEF + 32);
}

TEST_CASE("'CompressedChunkStorage<bool>' random values round-trip", "[voxen::land::compressed_chunk_storage]")
{
	test<bool>(0xDEADBEEF + 1);
}

} // namespace voxen::land
