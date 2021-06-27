#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <type_traits>

namespace voxen
{

enum class AllocationDomain {
	// Hermite data storage of terrain chunks primary data
	TerrainHermite,
	// Vertices and indices storage of terrain chunks
	TerrainMesh,
	// Octree nodes storage of terrain chunks secondary data
	TerrainOctree
};

// A helper class to control per-domain memory usage
template<AllocationDomain D>
class AllocationTracker final {
public:
	// Returns an estimated value of currently used memory
	static size_t currentlyUsedMemory() noexcept;
private:
	static std::atomic_size_t s_memoryUsage;

	static void increaseMemoryUsage(size_t value) noexcept;
	static void decreaseMemoryUsage(size_t value) noexcept;

	template<typename, AllocationDomain>
	friend class DomainAllocator;
};

// An allocator counting its memory usage in a given domain
template<typename T, AllocationDomain D>
class DomainAllocator : private std::allocator<T> {
public:
	using value_type = T;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using propagate_on_container_copy_assignment = std::true_type;
	using propagate_on_container_move_assignment = std::true_type;
	using propagate_on_container_swap = std::true_type;
	using is_always_equal = std::true_type;

	template<typename U> struct rebind {
		using other = DomainAllocator<U, D>;
	};

	constexpr DomainAllocator() = default;
	constexpr DomainAllocator(DomainAllocator &&) = default;
	constexpr DomainAllocator(const DomainAllocator &) = default;
	constexpr DomainAllocator &operator = (DomainAllocator &&) = default;
	constexpr DomainAllocator &operator = (const DomainAllocator &) = default;
	constexpr ~DomainAllocator() = default;

	[[nodiscard]] constexpr T *allocate(size_t n)
	{
		T *ptr = std::allocator<T>::allocate(n);
		AllocationTracker<D>::increaseMemoryUsage(sizeof(T) * n);
		return ptr;
	}

	constexpr void deallocate(T *p, size_t n) noexcept
	{
		std::allocator<T>::deallocate(p, n);
		AllocationTracker<D>::decreaseMemoryUsage(sizeof(T) * n);
	}

	constexpr bool operator == (const DomainAllocator &) const noexcept { return true; }
};

extern template class AllocationTracker<AllocationDomain::TerrainHermite>;
extern template class AllocationTracker<AllocationDomain::TerrainMesh>;
extern template class AllocationTracker<AllocationDomain::TerrainOctree>;

}
