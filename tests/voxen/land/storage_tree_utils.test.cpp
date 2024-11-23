#include <voxen/land/storage_tree_utils.hpp>

#include "../../voxen_test_common.hpp"

namespace voxen::land
{

// Wrapper around Catch CHECK to not spam assertions count (there are tons of them)
#define SILENT_CHECK(expr) \
	if (!(expr)) \
	CHECK(expr)

namespace
{

bool wrapXCompare(int64_t a, int64_t b)
{
	constexpr int64_t MOD = int64_t(Consts::STORAGE_TREE_ROOT_ITEM_SIZE_CHUNKS * Consts::STORAGE_TREE_ROOT_ITEMS_X);
	constexpr int64_t ADD = MOD / 2 + MOD * 10;
	return (a + ADD) % MOD == (b + ADD) % MOD;
}

bool wrapZCompare(int64_t a, int64_t b)
{
	constexpr int64_t MOD = int64_t(Consts::STORAGE_TREE_ROOT_ITEM_SIZE_CHUNKS * Consts::STORAGE_TREE_ROOT_ITEMS_Z);
	constexpr int64_t ADD = MOD / 2 + MOD * 10;
	return (a + ADD) % MOD == (b + ADD) % MOD;
}

} // namespace

TEST_CASE("'StorageTreeUtils' random round-trip key-path conversions", "[voxen::land::storage_tree_utils]")
{
	// Multiply min/max X/Z bounds by 4 to stress coordinate wraparounds
	constexpr int32_t MIN_X = Consts::MIN_UNIQUE_WORLD_X_CHUNK * 4;
	constexpr int32_t MIN_Z = Consts::MIN_UNIQUE_WORLD_Z_CHUNK * 4;
	constexpr int32_t MAX_X = Consts::MAX_UNIQUE_WORLD_X_CHUNK * 4;
	constexpr int32_t MAX_Z = Consts::MAX_UNIQUE_WORLD_Z_CHUNK * 4;

	std::uniform_int_distribution<int32_t> x_dist(MIN_X, MAX_X);
	std::uniform_int_distribution<int32_t> y_dist(Consts::MIN_WORLD_Y_CHUNK, Consts::MAX_WORLD_Y_CHUNK);
	std::uniform_int_distribution<int32_t> z_dist(MIN_Z, MAX_Z);
	std::uniform_int_distribution<uint32_t> scale_dist(0, Consts::NUM_LOD_SCALES - 1);

	std::mt19937 rng(0xDEADBEEF);

	for (int i = 0; i < 25'000; i++) {
		ChunkKey key;

		uint32_t scale = scale_dist(rng);
		// Mask off lower bits to align to chunk scale, works on negative signed too
		int32_t mask = static_cast<int32_t>(~((1u << scale) - 1));

		key.x = x_dist(rng) & mask;
		key.y = y_dist(rng) & mask;
		key.z = z_dist(rng) & mask;
		key.scale_log2 = scale;

		auto maybe_tree_path = StorageTreeUtils::keyToTreePath(key);
		// Key generated this way must be valid
		SILENT_CHECK(maybe_tree_path.has_value());

		ChunkKey restored_key = StorageTreeUtils::treePathToKey(*maybe_tree_path);
		INFO("Original key: (" << key.x << ", " << key.y << ", " << key.z << ") lod " << key.scale_log2);
		INFO("Restored key: (" << restored_key.x << ", " << restored_key.y << ", " << restored_key.z << ") lod "
									  << restored_key.scale_log2);

		SILENT_CHECK(key.scale_log2 == restored_key.scale_log2);
		SILENT_CHECK(key.y == restored_key.y);
		SILENT_CHECK(wrapXCompare(key.x, restored_key.x));
		SILENT_CHECK(wrapZCompare(key.z, restored_key.z));
	}
}

TEST_CASE("'StorageTreeUtils' select round-trip key-path conversions", "[voxen::land::storage_tree_utils]")
{
	auto check = [](int32_t x, int32_t y, int32_t z, uint32_t lod) {
		ChunkKey key(x, y, z, lod);

		auto maybe_tree_path = StorageTreeUtils::keyToTreePath(key);
		SILENT_CHECK(maybe_tree_path.has_value());

		ChunkKey restored_key = StorageTreeUtils::treePathToKey(*maybe_tree_path);
		INFO("Original key: (" << key.x << ", " << key.y << ", " << key.z << ") lod " << key.scale_log2);
		INFO("Restored key: (" << restored_key.x << ", " << restored_key.y << ", " << restored_key.z << ") lod "
									  << restored_key.scale_log2);

		SILENT_CHECK(key.scale_log2 == restored_key.scale_log2);
		SILENT_CHECK(key.y == restored_key.y);
		SILENT_CHECK(wrapXCompare(key.x, restored_key.x));
		SILENT_CHECK(wrapZCompare(key.z, restored_key.z));
	};

	check(0, 0, 0, 0);
	check(0, 1, 0, 0);
	check(0, 1, -1, 0);
	check(-1, 0, 0, 0);

	check(-2, 0, 0, 1);
	check(-4, 0, 0, 1);
	check(-4, 0, 0, 2);

	for (uint32_t lod = 0; lod < Consts::NUM_LOD_SCALES; lod++) {
		check(Consts::MIN_UNIQUE_WORLD_X_CHUNK, Consts::MIN_WORLD_Y_CHUNK, Consts::MIN_UNIQUE_WORLD_Z_CHUNK, lod);
		check(0, Consts::MIN_WORLD_Y_CHUNK, Consts::MIN_UNIQUE_WORLD_Z_CHUNK, lod);
		check(Consts::MIN_UNIQUE_WORLD_X_CHUNK, Consts::MIN_WORLD_Y_CHUNK, 0, lod);

		check(Consts::MAX_UNIQUE_WORLD_X_CHUNK + 1, 0, Consts::MAX_UNIQUE_WORLD_Z_CHUNK + 1, lod);
		check(0, 0, Consts::MAX_UNIQUE_WORLD_Z_CHUNK + 1, lod);
		check(Consts::MAX_UNIQUE_WORLD_X_CHUNK + 1, 0, 0, lod);
	}

	check((Consts::MAX_UNIQUE_WORLD_X_CHUNK + 1) * 2, 0, 0, 6);
	check(Consts::MIN_UNIQUE_WORLD_X_CHUNK * 2, 0, 0, 6);
	check(0, 0, (Consts::MAX_UNIQUE_WORLD_Z_CHUNK + 1) * 2, 6);
	check(0, 0, Consts::MIN_UNIQUE_WORLD_Z_CHUNK * 2, 6);
}

TEST_CASE("'StorageTreeUtils' invalid keys to path conversions", "[voxen::land::storage_tree_utils]")
{
	auto check = [](int32_t x, int32_t y, int32_t z, uint32_t lod) {
		CHECK(!StorageTreeUtils::keyToTreePath(ChunkKey(x, y, z, lod)).has_value());
	};

	// Out of height bounds
	check(0, Consts::MAX_WORLD_Y_CHUNK + 1, 0, 0);
	check(0, Consts::MAX_WORLD_Y_CHUNK * 2, 0, 0);
	check(0, Consts::MAX_WORLD_Y_CHUNK * 3, 0, 0);

	check(0, Consts::MIN_WORLD_Y_CHUNK - 1, 0, 0);
	check(0, Consts::MIN_WORLD_Y_CHUNK * 2, 0, 0);
	check(0, Consts::MIN_WORLD_Y_CHUNK * 3, 0, 0);

	// Too large scale
	check(0, 0, 0, Consts::NUM_LOD_SCALES);
	check(0, 0, 0, Consts::NUM_LOD_SCALES + 1);
	check(Consts::MAX_UNIQUE_WORLD_X_CHUNK + 100, 0, 0, Consts::NUM_LOD_SCALES);

	// Misaligned to power of two grid
	check(0, 0, 1, 1);
	check(0, -1, 0, 1);
	check(-1, 0, 0, 1);
	check(-4, -4, -4, 4);
	check(13, 0, 0, 3);
}

} // namespace voxen::land
