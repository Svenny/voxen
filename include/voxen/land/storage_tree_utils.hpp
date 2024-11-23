#pragma once

#include <voxen/land/chunk_key.hpp>
#include <voxen/visibility.hpp>

#include <optional>

// Common helper functions to use `StorageTree`
namespace voxen::land::StorageTreeUtils
{

// Translate `ChunkKey` into a bit-encoded `StorageTree` traverse path.
// This path is fixed for a given key and can be cached and used with any tree.
//
// Certain keys are invalid and cannot be used for `StorageTree` operations.
// For those `std::nullopt` (no value) will be returned. Invalid keys include:
// - Out of world height bounds, see `Consts::[MIN|MAX]_WORLD_Y_CHUNK`
// - LOD scale above the maximal supported level, see `Consts::NUM_LOD_SCALES`
// - Not aligned to the power of two grid for their scale, in other words
//   either of X/Y/Z key components is not divisible by `key.scaleMultiplier()`
//
// The latter two cases are almost certainly bugs in the program, while
// the former can be encountered normally when loading very high/deep chunks.
// So it should be enough to only check height bounds and act appropriately.
//
// This function is quite simple but still it does some computations.
// Avoid calling it every time if you can store the returned path.
VOXEN_API std::optional<uint64_t> keyToTreePath(ChunkKey key) noexcept;

// Reconstruct `ChunkKey` from a bit-encoded `StorageTree` traverse path.
// `tree_path` must be a valid value returned from `keyToTreePath()` call.
//
// Note that path conversion wraps X/Z coordinates (see `Consts::*_UNIQUE_WORLD_*_CHUNK`)
// and forms classes of key equivalence this way. If the original key was out of
// non-wrapping bounds, this function will return a different key pointing to the same data.
//
// In fact, you can even use encoded tree paths instead of keys as "better keys".
//
// This function is quite simple but still it does some computations.
// Avoid calling it every time if you can store the returned key.
VOXEN_API ChunkKey treePathToKey(uint64_t tree_path) noexcept;

} // namespace voxen::land::StorageTreeUtils
