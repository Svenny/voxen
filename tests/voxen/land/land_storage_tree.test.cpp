#include <voxen/land/land_storage_tree.hpp>

#include <voxen/land/storage_tree_utils.hpp>

#include "../../voxen_test_common.hpp"

#include <unordered_map>
#include <unordered_set>

namespace voxen::land
{

namespace
{

// Wrapper around Catch CHECK to not spam assertions count (there are tons of them)
#define SILENT_CHECK(expr) \
	if (!(expr)) \
	CHECK(expr)

std::unordered_map<void *, ChunkKey> g_live_keys;

struct ChunkUserData {
	ChunkKey my_key;
};

struct DuoctreeUserData {
	int junk[16] = {};
	// After to force different layout from chunk data
	ChunkKey my_key;
};

void chunkDefaultCtor(void *, ChunkKey key, void *place)
{
	SILENT_CHECK((uintptr_t) place % alignof(ChunkUserData) == 0);
	SILENT_CHECK(key.scale_log2 == 0);

	SILENT_CHECK(!g_live_keys.contains(place));

	auto *data = new (place) ChunkUserData;
	data->my_key = key;
	g_live_keys[place] = key;
}

void chunkCopyCtor(void *, ChunkKey key, void *place, void *copy_from)
{
	SILENT_CHECK((uintptr_t) place % alignof(ChunkUserData) == 0);

	SILENT_CHECK(!g_live_keys.contains(place));
	SILENT_CHECK(g_live_keys.contains(copy_from));

	auto *from = reinterpret_cast<ChunkUserData *>(copy_from);
	SILENT_CHECK(from->my_key == g_live_keys[copy_from]);
	SILENT_CHECK(from->my_key == key);

	new (place) ChunkUserData(*from);
	g_live_keys[place] = key;
}

void chunkDtor(void *, ChunkKey key, void *place) noexcept
{
	SILENT_CHECK(g_live_keys.contains(place));

	auto *data = reinterpret_cast<ChunkUserData *>(place);
	SILENT_CHECK(data->my_key == g_live_keys[place]);
	SILENT_CHECK(data->my_key == key);

	data->~ChunkUserData();
	g_live_keys.erase(place);
}

void duoctreeDefaultCtor(void *, ChunkKey key, void *place)
{
	SILENT_CHECK((uintptr_t) place % alignof(ChunkUserData) == 0);
	SILENT_CHECK(key.scale_log2 > 0);
	// Duoctree has direct nodes only for even LODs
	SILENT_CHECK(key.scale_log2 % 2 == 0);

	SILENT_CHECK(!g_live_keys.contains(place));

	auto *data = new (place) DuoctreeUserData;
	data->my_key = key;
	g_live_keys[place] = key;
}

void duoctreeCopyCtor(void *, ChunkKey key, void *place, void *copy_from)
{
	SILENT_CHECK((uintptr_t) place % alignof(ChunkUserData) == 0);

	SILENT_CHECK(!g_live_keys.contains(place));
	SILENT_CHECK(g_live_keys.contains(copy_from));

	auto *from = reinterpret_cast<DuoctreeUserData *>(copy_from);
	SILENT_CHECK(from->my_key == g_live_keys[copy_from]);
	SILENT_CHECK(from->my_key == key);

	new (place) DuoctreeUserData(*from);
	g_live_keys[place] = key;
}

void duoctreeDtor(void *, ChunkKey key, void *place) noexcept
{
	SILENT_CHECK(g_live_keys.contains(place));

	auto *data = reinterpret_cast<DuoctreeUserData *>(place);
	SILENT_CHECK(data->my_key == g_live_keys[place]);
	SILENT_CHECK(data->my_key == key);

	data->~DuoctreeUserData();
	g_live_keys.erase(place);
}

constexpr StorageTreeControl ST_CTL = {
	.chunk_user_data_size = sizeof(ChunkUserData),
	.duoctree_user_data_size = sizeof(DuoctreeUserData),
	.user_fn_ctx = nullptr,
	.chunk_user_data_default_ctor = chunkDefaultCtor,
	.chunk_user_data_copy_ctor = chunkCopyCtor,
	.chunk_user_data_dtor = chunkDtor,
	.duoctree_user_data_default_ctor = duoctreeDefaultCtor,
	.duoctree_user_data_copy_ctor = duoctreeCopyCtor,
	.duoctree_user_data_dtor = duoctreeDtor,
};

std::vector<ChunkKey> geneateUniqueKeys(size_t num, std::mt19937 &rng)
{
	// Multiply min/max X/Z bounds by 2 to stress coordinate wraparounds
	std::uniform_int_distribution<int32_t> x_dist(Consts::MIN_UNIQUE_WORLD_X_CHUNK * 2,
		Consts::MAX_UNIQUE_WORLD_X_CHUNK * 2);
	std::uniform_int_distribution<int32_t> y_dist(Consts::MIN_WORLD_Y_CHUNK, Consts::MAX_WORLD_Y_CHUNK);
	std::uniform_int_distribution<int32_t> z_dist(Consts::MIN_UNIQUE_WORLD_Z_CHUNK * 2,
		Consts::MAX_UNIQUE_WORLD_Z_CHUNK * 2);
	std::uniform_int_distribution<uint32_t> scale_dist(0, Consts::NUM_LOD_SCALES - 1);

	std::unordered_set<uint64_t> tree_paths;

	std::vector<ChunkKey> keys(num);

	for (auto &key : keys) {
		while (true) {
			uint32_t scale = scale_dist(rng);
			// Mask off lower bits to align to chunk scale, works on negative signed too
			int32_t mask = static_cast<int32_t>(~((1u << scale) - 1));

			key.x = x_dist(rng) & mask;
			key.y = y_dist(rng) & mask;
			key.z = z_dist(rng) & mask;
			key.scale_log2 = scale;

			auto maybe_tree_path = StorageTreeUtils::keyToTreePath(key);
			// Our generated key must be valid
			SILENT_CHECK(maybe_tree_path.has_value());

			// Repeat generation if it's not unique
			if (tree_paths.emplace(*maybe_tree_path).second) {
				break;
			}
		}
	}

	return keys;
}

} // namespace

TEST_CASE("'StorageTree' test case 1 (insertions)", "[voxen::land::land_storage_tree]")
{
	auto st = std::make_unique<StorageTree>(ST_CTL);

	CHECK(Consts::MIN_UNIQUE_WORLD_X_CHUNK < 0);
	CHECK(Consts::MIN_WORLD_Y_CHUNK < 0);
	CHECK(Consts::MIN_UNIQUE_WORLD_Z_CHUNK < 0);

	CHECK(Consts::MAX_UNIQUE_WORLD_X_CHUNK > 0);
	CHECK(Consts::MAX_WORLD_Y_CHUNK > 0);
	CHECK(Consts::MAX_UNIQUE_WORLD_Z_CHUNK > 0);

	std::mt19937 rng(0xDEADBEEF + 1);
	auto test_keys = geneateUniqueKeys(15'000, rng);

	// Cache tree paths, keys won't be needed again
	std::vector<uint64_t> tree_paths(test_keys.size());
	for (size_t i = 0; i < test_keys.size(); i++) {
		tree_paths[i] = *StorageTreeUtils::keyToTreePath(test_keys[i]);
	}

	// Do several "epochs" of overwriting all keys
	for (int64_t epoch = 1; epoch <= 5; epoch++) {
		WorldTickId tick(epoch);

		for (uint64_t path : tree_paths) {
			void *ptr = st->access(path, tick);
			SILENT_CHECK(ptr != nullptr);
			SILENT_CHECK(g_live_keys.contains(ptr));
		}

		// Make things a bit less predictable
		std::shuffle(std::begin(tree_paths), std::end(tree_paths), rng);
	}

	st.reset();
	CHECK(g_live_keys.size() == 0);
}

TEST_CASE("'StorageTree' test case 2 (lookups)", "[voxen::land::land_storage_tree]")
{
	auto st = std::make_unique<StorageTree>(ST_CTL);

	std::mt19937 rng(0xDEADBEEF + 2);
	auto test_keys = geneateUniqueKeys(5'000, rng);

	WorldTickId tick(1);

	// Insert only even keys
	for (size_t i = 0; i < test_keys.size(); i += 2) {
		st->access(*StorageTreeUtils::keyToTreePath(test_keys[i]), tick);
	}

	// Look up all keys - even ones should be found, odd ones should not
	for (size_t i = 0; i < test_keys.size(); i++) {
		void *ptr = st->lookup(*StorageTreeUtils::keyToTreePath(test_keys[i]));
		if (i % 2 == 0) {
			SILENT_CHECK(ptr != nullptr);
			SILENT_CHECK(g_live_keys.contains(ptr));
		} else if (ptr != nullptr) {
			// Load actual (wrapped) key
			ChunkKey key = (test_keys[i].scale_log2 == 0
					? reinterpret_cast<ChunkUserData *>(ptr)->my_key
					: reinterpret_cast<DuoctreeUserData *>(ptr)->my_key);

			INFO("Found node data that shouldn't be found: ("
				<< key.x << ", " << key.y << ", " << key.z << ") lod " << key.scale_log2);
			CHECK(ptr == nullptr);
		}
	}
}

TEST_CASE("'StorageTree' test case 3 (removals)", "[voxen::land::land_storage_tree]")
{
	auto st = std::make_unique<StorageTree>(ST_CTL);

	std::mt19937 rng(0xDEADBEEF + 3);

	auto test_keys = geneateUniqueKeys(5'000, rng);

	// Cache tree paths, keys won't be needed again
	std::vector<uint64_t> tree_paths(test_keys.size());
	for (size_t i = 0; i < test_keys.size(); i++) {
		tree_paths[i] = *StorageTreeUtils::keyToTreePath(test_keys[i]);
	}

	// Do several rounds of inserting and removing keys
	for (int64_t epoch = 1; epoch <= 3; epoch++) {
		WorldTickId tick(epoch);

		for (uint64_t path : tree_paths) {
			st->remove(path, tick);
		}

		// Everything was just removed
		CHECK(g_live_keys.empty());

		for (uint64_t path : tree_paths) {
			void *ptr = st->access(path, tick);
			SILENT_CHECK(ptr != nullptr);
			SILENT_CHECK(g_live_keys.contains(ptr));
		}

		// Make things a bit less predictable
		std::shuffle(std::begin(tree_paths), std::end(tree_paths), rng);
	}
}

} // namespace voxen::land
