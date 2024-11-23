#pragma once

#include <voxen/land/chunk_key.hpp>

#include <cstdint>

namespace voxen::land
{

// Type erasure information for `StorageTree`.
// Do not fill it directly, use `TypedStorageTree` instead.
struct StorageTreeControl {
	// Size (bytes) of user data block attached to chunk nodes
	uint32_t chunk_user_data_size = 0;
	// Size (bytes) of user data block attached to duoctree nodes
	uint32_t duoctree_user_data_size = 0;
	// `ctx` pointer passed as is to every function below
	void *user_fn_ctx = nullptr;

	// Initial constructor of a chunk node user data block
	void (*chunk_user_data_default_ctor)(void *ctx, ChunkKey key, void *place) = nullptr;
	// Copy constructor of a chunk node user data block
	void (*chunk_user_data_copy_ctor)(void *ctx, ChunkKey key, void *place, void *copy_from) = nullptr;
	// Destructor of a chunk node user data block
	void (*chunk_user_data_dtor)(void *ctx, ChunkKey key, void *place) noexcept = nullptr;

	// Initial constructor of a duoctree node user data block
	void (*duoctree_user_data_default_ctor)(void *ctx, ChunkKey key, void *place) = nullptr;
	// Copy constructor of a duoctree node user data block
	void (*duoctree_user_data_copy_ctor)(void *ctx, ChunkKey key, void *place, void *copy_from) = nullptr;
	// Destructor of a duoctree node user data block
	void (*duoctree_user_data_dtor)(void *ctx, ChunkKey key, void *place) noexcept = nullptr;
};

} // namespace voxen::land
