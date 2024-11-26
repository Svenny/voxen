#pragma once

#include <voxen/visibility.hpp>

#include <bit>
#include <memory>
#include <type_traits>

namespace voxen
{

namespace detail
{

// Base implementation for `PrivateObjectPool<T>`, do not use directly
class VOXEN_API PrivateObjectPoolBase {
public:
	constexpr static size_t MAX_OBJECT_SIZE = 512;
	constexpr static size_t MAX_OBJECT_ALIGN = 64;
	constexpr static size_t SLAB_HEADER_SIZE = 2 * sizeof(void *) + 8;

protected:
	PrivateObjectPoolBase(size_t object_size, size_t objects_hint) noexcept;
	PrivateObjectPoolBase(PrivateObjectPoolBase &&) = delete;
	PrivateObjectPoolBase(const PrivateObjectPoolBase &) = delete;
	PrivateObjectPoolBase &operator=(PrivateObjectPoolBase &&) = delete;
	PrivateObjectPoolBase &operator=(const PrivateObjectPoolBase &) = delete;
	~PrivateObjectPoolBase();

	void *allocate();
	static void deallocate(void *obj, size_t slab_size) noexcept;

	constexpr static size_t adjustObjectSize(size_t object_size) noexcept
	{
		return std::max(object_size, sizeof(void *));
	}

	constexpr static size_t calcSlabSize(size_t object_size, size_t objects_hint) noexcept
	{
		constexpr size_t ptr_size = sizeof(void *);
		size_t slab_size = objects_hint * std::max(object_size, ptr_size);
		// Align header start to the pointer size
		slab_size = (slab_size + ptr_size - 1u) & ~(ptr_size - 1u);
		slab_size += SLAB_HEADER_SIZE;
		return std::bit_ceil(slab_size);
	}

private:
	const uint32_t m_adjusted_object_size;
	const uint32_t m_slab_size;
	const uint32_t m_max_objects;
	uint32_t m_live_allocations = 0;

	void *m_last_freed_object = nullptr;
	void *m_newest_slab = nullptr;
};

} // namespace detail

// The simplest unbounded object pool using a list of fixed-size "slabs".
// Allocates objects with unique ownership. This pool is NOT thread-safe.
//
// Not efficient for extremely tiny objects - allocations are rounded
// up to one pointer size (4/8 bytes) for internal bookkeeping.
// These objects should be stored inline where possible anyway.
//
// `SLAB_SIZE_HINT` controls how many objects should be placed in one "slab" memory block.
// The implementation might allocate more than this number but will not allocate less.
// You can tweak it based on the expected number of simultaneous live object instances.
//
// Also see `SharedObjectPool`.
template<typename T, uint32_t SLAB_SIZE_HINT = 256>
class PrivateObjectPool : private detail::PrivateObjectPoolBase {
public:
	using Base = detail::PrivateObjectPoolBase;

	struct Deleter {
		void operator()(T *obj) noexcept
		{
			constexpr size_t SLAB_SIZE = Base::calcSlabSize(sizeof(T), SLAB_SIZE_HINT);

			obj->~T();
			Base::deallocate(obj, SLAB_SIZE);
		}
	};

	// Handle-like pointer to the allocated object
	using Ptr = std::unique_ptr<T, Deleter>;

	PrivateObjectPool() noexcept : PrivateObjectPoolBase(sizeof(T), SLAB_SIZE_HINT) {}
	PrivateObjectPool(PrivateObjectPool &&) = delete;
	PrivateObjectPool(const PrivateObjectPool &) = delete;
	PrivateObjectPool &operator=(PrivateObjectPool &&) = delete;
	PrivateObjectPool &operator=(const PrivateObjectPool &) = delete;
	// NOTE: all allocated objects must be destroyed before the pool
	~PrivateObjectPool() = default;

	// Allocate and construct an object, similar to `std::make_unique()`
	template<typename... Args>
	Ptr allocate(Args &&...args)
	{
		static_assert(sizeof(T) <= Base::MAX_OBJECT_SIZE, "It's so big!");
		static_assert(alignof(T) <= Base::MAX_OBJECT_ALIGN, "Pooled object is aligned too strictly");

		constexpr size_t SLAB_SIZE = Base::calcSlabSize(sizeof(T), SLAB_SIZE_HINT);

		void *place = Base::allocate();
		if constexpr (!std::is_nothrow_constructible_v<T, Args...>) {
			try {
				return Ptr(new (place) T(std::forward<Args>(args)...));
			}
			catch (...) {
				Base::deallocate(place, SLAB_SIZE);
				throw;
			}
		} else {
			return Ptr(new (place) T(std::forward<Args>(args)...));
		}
	}
};

} // namespace voxen
