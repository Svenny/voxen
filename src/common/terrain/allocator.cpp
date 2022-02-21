#include <voxen/common/terrain/allocator.hpp>

#include <voxen/common/terrain/chunk_octree.hpp>
#include <voxen/common/terrain/config.hpp>
#include <voxen/common/terrain/primary_data.hpp>
#include <voxen/common/terrain/surface.hpp>
#include <voxen/util/allocator.hpp>
#include <voxen/util/debug.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <extras/fixed_pool.hpp>

#include <list>

namespace voxen::terrain
{

namespace
{

template<typename T, bool R, AllocationDomain D>
struct Superpool final {
public:
	constexpr static size_t SUBPOOL_SIZE = Config::ALLOCATION_SUBPOOL_SIZE;

	using Subpool = extras::fixed_pool<T, SUBPOOL_SIZE, R>;
	using SubpoolPtr = std::unique_ptr<Subpool>;
	using ObjectPtr = typename Subpool::pointer;

	Superpool() = default;
	Superpool(Superpool &&) = delete;
	Superpool(const Superpool &) = delete;
	Superpool &operator = (Superpool &&) = delete;
	Superpool &operator = (const Superpool &) = delete;
	~Superpool() = default;

	// TODO: not thread-safe
	template<typename... Args>
	ObjectPtr allocate(Args&&... args)
	{
		// TODO: allocation complexity will increase as pool gets occupied
		for (auto &subpool : m_subpools) {
			auto ptr = subpool.allocate(std::forward<Args>(args)...);
			if (ptr) {
				return ptr;
			}
		}

		auto &new_pool = m_subpools.emplace_back();

		auto name = DebugUtils::demangle<T>();
		Log::debug("PoolAllocator<{}>: no free subpools left, growing to {}", name.get(), m_subpools.size());

		// Newly allocated subpool is guaranteed to have free space :)
		return new_pool.allocate(std::forward<Args>(args)...);
	}

	void gcFast() noexcept
	{
		if (m_subpools.empty()) {
			return;
		}

		// TODO: add heuristic to leave some empty subpools allocated
		// when there is low free space in partially-occupied ones.
		// This will prevent possible frequent allocation/deallocation.
		if (m_fast_gc_iter == m_subpools.end()) {
			m_fast_gc_iter = m_subpools.begin();
		}

		if (m_fast_gc_iter->free_space() == SUBPOOL_SIZE) {
			m_fast_gc_iter = m_subpools.erase(m_fast_gc_iter);
		} else {
			++m_fast_gc_iter;
		}
	}

	void gcFull() noexcept
	{
		for (auto iter = m_subpools.begin(); iter != m_subpools.end(); /*no action*/) {
			if (iter->free_space() == SUBPOOL_SIZE) {
				iter = m_subpools.erase(iter);
			} else {
				++iter;
			}
		}

		m_fast_gc_iter = m_subpools.begin();
	}

private:
	using ListType = std::list<Subpool, DomainAllocator<Subpool, D>>;

	ListType m_subpools;
	typename ListType::iterator m_fast_gc_iter = m_subpools.end();
};

}

static Superpool<Chunk, false, AllocationDomain::TerrainPrimary> g_chunk_superpool;
static Superpool<ChunkPrimaryData, true, AllocationDomain::TerrainPrimary> g_primary_superpool;
static Superpool<ChunkOctree, true, AllocationDomain::TerrainOctree> g_octree_superpool;
static Superpool<ChunkOwnSurface, true, AllocationDomain::TerrainMesh> g_own_surface_superpool;
static Superpool<ChunkSeamSurface, true, AllocationDomain::TerrainMesh> g_seam_surface_superpool;

PoolAllocator::ChunkPtr PoolAllocator::allocateChunk(Chunk::CreationInfo info)
{
	return g_chunk_superpool.allocate(std::move(info));
}

PoolAllocator::PrimaryDataPtr PoolAllocator::allocatePrimaryData()
{
	return g_primary_superpool.allocate();
}

PoolAllocator::OctreePtr PoolAllocator::allocateOctree()
{
	return g_octree_superpool.allocate();
}

PoolAllocator::OwnSurfacePtr PoolAllocator::allocateOwnSurface()
{
	return g_own_surface_superpool.allocate();
}

PoolAllocator::SeamSurfacePtr PoolAllocator::allocateSeamSurface()
{
	return g_seam_surface_superpool.allocate();
}

void PoolAllocator::collectGarbage() noexcept
{
	g_chunk_superpool.gcFast();
	g_primary_superpool.gcFast();
	g_octree_superpool.gcFast();
	g_own_surface_superpool.gcFast();
	g_seam_surface_superpool.gcFast();
}

}
