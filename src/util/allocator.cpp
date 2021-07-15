#include <voxen/util/allocator.hpp>

namespace voxen
{

template<AllocationDomain D>
std::atomic_size_t AllocationTracker<D>::s_memoryUsage = 0;

template<AllocationDomain D>
size_t AllocationTracker<D>::currentlyUsedMemory() noexcept
{
	return s_memoryUsage.load(std::memory_order_relaxed);
}

template<AllocationDomain D>
void AllocationTracker<D>::increaseMemoryUsage(size_t value) noexcept
{
	s_memoryUsage.fetch_add(value, std::memory_order_relaxed);
}

template<AllocationDomain D>
void AllocationTracker<D>::decreaseMemoryUsage(size_t value) noexcept
{
	s_memoryUsage.fetch_sub(value, std::memory_order_relaxed);
}

template class AllocationTracker<AllocationDomain::TerrainMesh>;
template class AllocationTracker<AllocationDomain::TerrainOctree>;
template class AllocationTracker<AllocationDomain::TerrainPrimary>;
template class AllocationTracker<AllocationDomain::StandbyCache>;

}
