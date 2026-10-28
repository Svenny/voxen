#include <voxen/land/compressed_chunk_storage.hpp>

#include <voxen/land/cube_array.hpp>
#include <voxen/land/land_consts.hpp>
#include <voxen/land/land_utils.hpp>

#include "../../voxen_test_common.hpp"

#include <cstdint>
#include <random>
#include <type_traits>

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
		// Fill `source` with random values. For `bool` a plain cast is almost
		// always `true`, which would never produce a non-uniform node.
		for (auto &item : *source) {
			if constexpr (std::is_same_v<T, bool>) {
				item = (rng() % 3) == 0;
			} else {
				item = static_cast<T>(rng());
			}
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

// Copy construction and assignment must reproduce the source
// and must leave it intact (a copy once zeroed the source's leaves).
template<typename T>
void testCopy(uint32_t seed)
{
	auto source = std::make_unique<CubeArray<T, N>>();
	auto dest = std::make_unique<CubeArray<T, N>>();

	std::mt19937 rng(seed);

	// Sparse pattern: zero, uniform and non-uniform 8x8x8 nodes all present
	source->fill(0);
	Utils::forYXZ<N>([&](uint32_t x, uint32_t y, uint32_t z) {
		if (x < 8 && y < 8 && z < 8) {
			source->store(x, y, z, static_cast<T>(7));
		} else if (x >= 16 && (rng() % 4) == 0) {
			source->store(x, y, z, static_cast<T>(rng()));
		}
	});

	CompressedChunkStorage<T> storage(source->cview());
	REQUIRE(!storage.uniform());

	CompressedChunkStorage<T> copied(storage);
	copied.expand(dest->view());
	CHECK(*source == *dest);

	CompressedChunkStorage<T> assigned;
	assigned = storage;
	assigned.expand(dest->view());
	CHECK(*source == *dest);

	// The source storage must be unchanged after being copied from
	storage.expand(dest->view());
	CHECK(*source == *dest);

	// Uniform storage copies too
	source->fill(static_cast<T>(3));
	CompressedChunkStorage<T> uniform(source->cview());
	REQUIRE(uniform.uniform());
	CompressedChunkStorage<T> uniform_copy(uniform);
	uniform_copy.expand(dest->view());
	CHECK(*source == *dest);
}

} // namespace

TEST_CASE("'CompressedChunkStorage' copy", "[voxen::land::compressed_chunk_storage]")
{
	testCopy<uint8_t>(0xC0FFEE + 8);
	testCopy<uint16_t>(0xC0FFEE + 16);
	testCopy<uint32_t>(0xC0FFEE + 32);
	testCopy<bool>(0xC0FFEE + 1);
}

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
