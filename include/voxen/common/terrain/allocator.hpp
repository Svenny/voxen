#pragma once

#include <voxen/common/terrain/chunk.hpp>

#include <extras/refcnt_ptr.hpp>

namespace voxen::terrain
{

class ChunkOctree;
class ChunkOwnSurface;
class ChunkSeamSurface;
struct ChunkPrimaryData;

// This static class manages allocations for different kinds of objects from terrain subsystem
// TODO: currently allocation is not thread-safe, but returned pointers may be used from multiple threads
class PoolAllocator final {
public:
	using ChunkPtr = extras::refcnt_ptr<Chunk>;
	using PrimaryDataPtr = extras::refcnt_ptr<ChunkPrimaryData>;
	using OctreePtr = extras::refcnt_ptr<ChunkOctree>;
	using OwnSurfacePtr = extras::refcnt_ptr<ChunkOwnSurface>;
	using SeamSurfacePtr = extras::refcnt_ptr<ChunkSeamSurface>;

	// Static class, no instances of it are allowed
	PoolAllocator() = delete;

	static ChunkPtr allocateChunk(Chunk::CreationInfo info);
	static PrimaryDataPtr allocatePrimaryData();
	static OctreePtr allocateOctree();
	static OwnSurfacePtr allocateOwnSurface();
	static SeamSurfacePtr allocateSeamSurface();

	static void collectGarbage() noexcept;
};

}
