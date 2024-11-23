#pragma once

#include <voxen/land/storage_tree_utils.hpp>

// BMI bit hacking intrinsics
#include <immintrin.h>

namespace voxen::land::StorageTreeUtils
{

inline glm::ivec3 calcRootItemMinCoord(uint32_t index) noexcept
{
	uint32_t min_x_unshifted = index / Consts::STORAGE_TREE_ROOT_ITEMS_Z * Consts::STORAGE_TREE_ROOT_ITEM_SIZE_CHUNKS;
	uint32_t min_z_unshifted = index % Consts::STORAGE_TREE_ROOT_ITEMS_Z * Consts::STORAGE_TREE_ROOT_ITEM_SIZE_CHUNKS;

	glm::ivec3 min_coord(min_x_unshifted, Consts::MIN_WORLD_Y_CHUNK, min_z_unshifted);
	min_coord.x += Consts::MIN_UNIQUE_WORLD_X_CHUNK;
	min_coord.z += Consts::MIN_UNIQUE_WORLD_Z_CHUNK;

	return min_coord;
}

template<uint32_t B>
inline uint64_t extractNodePathComponent(uint64_t tree_path) noexcept
{
	// Extract B-th byte in one operation
	return _bextr_u64(tree_path, 8 * B, 8);
}

inline bool triquadtreeYNegative(uint64_t path_component) noexcept
{
	return !!(path_component & 64u);
}

inline uint64_t extractNodePathChildBit(uint64_t path_component) noexcept
{
	return uint64_t(1) << (path_component & 63u);
}

inline bool extractNodePathStopBit(uint64_t path_component) noexcept
{
	return !!(path_component & 128u);
}

inline uint32_t extractNodeKeyMaskBit(uint64_t tree_path, uint64_t path_component) noexcept
{
	if (path_component & 64u) {
		// Subnode bit set, use selector index
		return 1u << (tree_path & 7u);
	}

	// Set bit 8
	return 256u;
}

} // namespace voxen::land::StorageTreeUtils
