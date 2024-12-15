#pragma once

#include <voxen/visibility.hpp>

#include <algorithm>
#include <atomic>
#include <bit>
#include <utility>

namespace voxen
{

namespace detail
{

// Base implementation of `SharedObjectPool<T>`, do not use directly
class VOXEN_API SharedObjectPoolBase {
public:
	constexpr static size_t MAX_OBJECT_SIZE = 512;
	constexpr static size_t MAX_OBJECT_ALIGN = 64;
	constexpr static size_t SLAB_HEADER_SIZE = 2 * sizeof(void *) + 8;

	static void addRef(void *obj, size_t slab_size, size_t adjusted_object_size) noexcept;
	static bool releaseRef(void *obj, size_t slab_size, size_t adjusted_object_size) noexcept;
	static void deallocate(void *obj, size_t slab_size) noexcept;

	constexpr static size_t adjustObjectSize(size_t object_size) noexcept
	{
		return std::max(object_size, sizeof(void *));
	}

	constexpr static size_t calcSlabSize(size_t object_size, size_t objects_hint) noexcept
	{
		constexpr size_t ptr_size = sizeof(void *);
		// Add one byte per object for refcounts
		size_t slab_size = objects_hint * std::max(object_size, ptr_size) + objects_hint;
		// Align header start to the pointer size
		slab_size = (slab_size + ptr_size - 1u) & ~(ptr_size - 1u);
		slab_size += SLAB_HEADER_SIZE;
		return std::bit_ceil(slab_size);
	}

protected:
	SharedObjectPoolBase(size_t object_size, size_t objects_hint) noexcept;
	SharedObjectPoolBase(SharedObjectPoolBase &&) = delete;
	SharedObjectPoolBase(const SharedObjectPoolBase &) = delete;
	SharedObjectPoolBase &operator=(SharedObjectPoolBase &&) = delete;
	SharedObjectPoolBase &operator=(const SharedObjectPoolBase &) = delete;
	~SharedObjectPoolBase();

	void *allocate();

private:
	const uint32_t m_adjusted_object_size;
	const uint32_t m_slab_size;
	const uint32_t m_max_objects;

	std::atomic<void *> m_last_freed_object = nullptr;
	void *m_newest_slab = nullptr;
};

} // namespace detail

// Smart reference-counter pointer to an object allocated from `SharedObjectPool`.
// Behaves pretty much like `std::shared_ptr` - the pointer object itself
// is not thread-safe but the same object can be referenced from multiple threads.
//
// Weak pointers are currently not supported. There is no way to make cyclic references.
// However, this is not a technical limitation, and it can be lifted in future.
//
// Another notable restriction in the maximal reference count. We don't expect pooled
// objects to have many live references, so currently their count is stored in uint16.
// Therefore, up to 65535 references to the same object can exist simultaneously.
// If this limit is exceeded, the engine will crash and request reporting a bug.
// Yes, you will *not* have to deal with undebuggable memory leak/corruption/whatever.
//
// This is also not a technical limitation but rather an opportunistic
// optimization, and it can be trivially changed.
template<typename T, uint32_t SLAB_SIZE_HINT = 256>
class SharedPoolPtr {
public:
	using PtrConst = SharedPoolPtr<std::add_const_t<T>, SLAB_SIZE_HINT>;
	using PtrNonConst = SharedPoolPtr<std::remove_const_t<T>, SLAB_SIZE_HINT>;
	constexpr static bool IS_CONST = std::is_same_v<T, std::add_const_t<T>>;

	// Default constructor, initializes to null pointer
	SharedPoolPtr() = default;

	// Move constructor from const pointer
	SharedPoolPtr(PtrConst &&other) noexcept
		requires(IS_CONST)
		: m_object(std::exchange(other.m_object, nullptr))
	{}

	// Move constructor from non-const pointer
	SharedPoolPtr(PtrNonConst &&other) noexcept : m_object(std::exchange(other.m_object, nullptr)) {}

	// Copy constructor from const pointer
	SharedPoolPtr(const PtrConst &other) noexcept
		requires(IS_CONST)
		: m_object(other.m_object)
	{
		addRef();
	}

	// Copy constructor from non-const pointer
	SharedPoolPtr(const PtrNonConst &other) noexcept : m_object(other.m_object) { addRef(); }

	// Move assignment from const pointer
	SharedPoolPtr &operator=(PtrConst &&other) noexcept
		requires(IS_CONST)
	{
		std::swap(m_object, other.m_object);
		return *this;
	}

	// Move assignment from non-const pointer
	SharedPoolPtr &operator=(PtrNonConst &&other) noexcept
	{
		std::swap(m_object, other.m_object);
		return *this;
	}

	// Copy assignment from const pointer
	SharedPoolPtr &operator=(const PtrConst &other) noexcept
		requires(IS_CONST)
	{
		if (m_object != other.m_object) {
			reset();
			m_object = other.m_object;
			addRef();
		}
		return *this;
	}

	// Copy assignment from non-const pointer
	SharedPoolPtr &operator=(const PtrNonConst &other) noexcept
	{
		if (m_object != other.m_object) {
			reset();
			m_object = other.m_object;
			addRef();
		}
		return *this;
	}

	~SharedPoolPtr() { reset(); }

	operator bool() const noexcept { return m_object != nullptr; }
	T *get() const noexcept { return m_object; }
	T *operator->() const noexcept { return m_object; }
	T &operator*() const noexcept { return *m_object; }

	void reset() noexcept
	{
		if (m_object) {
			constexpr size_t ADJUSTED_OBJ_SIZE = detail::SharedObjectPoolBase::adjustObjectSize(sizeof(T));
			constexpr size_t SLAB_SIZE = detail::SharedObjectPoolBase::calcSlabSize(sizeof(T), SLAB_SIZE_HINT);
			if (detail::SharedObjectPoolBase::releaseRef(m_object, SLAB_SIZE, ADJUSTED_OBJ_SIZE)) {
				m_object->~T();
				detail::SharedObjectPoolBase::deallocate(m_object, SLAB_SIZE);
			}

			m_object = nullptr;
		}
	}

private:
	T *m_object = nullptr;

	// Initialization (object must already have one ref added)
	explicit SharedPoolPtr(T *object) noexcept : m_object(object) {}

	void addRef() noexcept
	{
		if (m_object) {
			constexpr size_t ADJUSTED_OBJ_SIZE = detail::SharedObjectPoolBase::adjustObjectSize(sizeof(T));
			constexpr size_t SLAB_SIZE = detail::SharedObjectPoolBase::calcSlabSize(sizeof(T), SLAB_SIZE_HINT);
			detail::SharedObjectPoolBase::addRef(m_object, SLAB_SIZE, ADJUSTED_OBJ_SIZE);
		}
	}

	template<typename, uint32_t>
	friend class SharedObjectPool;
};

// A simple unbounded object pool using a list of fixed-size "slabs".
// Allocates objects with shared ownership. This pool is NOT thread-safe
// but the returned pointers can be used from multiple threads,
// i.e. object reference counting and deallocation IS thread-safe.
//
// Not efficient for extremely tiny objects - allocations are rounded
// up to one pointer size (4/8 bytes) for internal bookkeeping.
// These objects should be stored inline where possible anyway.
//
// `SLAB_SIZE_HINT` controls how many objects should be placed in one "slab" memory block.
// The implementation might allocate more than this number but will not allocate less.
// You can tweak it based on the expected number of simultaneous live object instances.
//
// Also see `PrivateObjectPool`.
template<typename T, uint32_t SLAB_SIZE_HINT = 256>
class SharedObjectPool : private detail::SharedObjectPoolBase {
public:
	using Base = detail::SharedObjectPoolBase;
	// Handle-like reference-counting pointer to the allocated object
	using Ptr = SharedPoolPtr<T, SLAB_SIZE_HINT>;

	SharedObjectPool() noexcept : SharedObjectPoolBase(sizeof(T), SLAB_SIZE_HINT) {}
	SharedObjectPool(SharedObjectPool &&) = delete;
	SharedObjectPool(const SharedObjectPool &) = delete;
	SharedObjectPool &operator=(SharedObjectPool &&) = delete;
	SharedObjectPool &operator=(const SharedObjectPool &) = delete;
	// NOTE: all allocated objects must be destroyed before the pool
	~SharedObjectPool() = default;

	// Allocate and construct an object, similar to `std::make_shared()`
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
