#pragma once

#include <voxen/common/terrain/chunk.hpp>

#include <extras/refcnt_ptr.hpp>

namespace voxen::terrain
{

// This static class manages allocations for different kinds of objects from terrain subsystem
// TODO: currently allocation is not thread-safe, but returned pointers may be used from multiple threads
class PoolAllocator final {
public:
	using ChunkPtr = extras::refcnt_ptr<Chunk>;

	// Static class, no instances of it are allowed
	PoolAllocator() = delete;

	static ChunkPtr allocateChunk(Chunk::CreationInfo info);

	static void collectGarbage() noexcept;
};

} // namespace voxen::terrain
