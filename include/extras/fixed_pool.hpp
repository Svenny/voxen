#pragma once

#include <extras/bitset.hpp>
#include <extras/futex.hpp>
#include <extras/refcnt_ptr.hpp>

#include <array>
#include <atomic>
#include <cassert>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>

namespace extras
{

namespace detail
{

template<typename T, uint32_t N, bool R>
struct fixed_pool_storage;

template<typename T, uint32_t N>
struct fixed_pool_storage<T, N, false> {
	using type = std::array<std::aligned_storage_t<sizeof(T), alignof(T)>, N>;
};

template<typename T, uint32_t N>
struct fixed_pool_storage<T, N, true> {
	static_assert(std::is_nothrow_default_constructible_v<T>,
		"Managed object in reusable pool must be nothrow default constructible");
	using type = std::array<T, N>;
};

} // namespace detail

// A thread-safe pool holding up to `N` objects of type `T`. It returns
// reference-counted pointers which will automatically recycle the object.
// WARNING: do not introduce cyclic pointer dependencies. This is not
// manageable by reference counting and will lead to memory leak.
// WARNING: only 255 pointers to the same object are allowed to exist
// simultaneously. Exceeding this limit leads to undefined behavior.
template<typename T, uint32_t N, bool R = false>
class fixed_pool final {
public:
	static_assert(std::is_nothrow_destructible_v<T>, "Managed object must be nothrow destructible");
	static_assert(!std::is_array_v<T>, "Managing C arrays is not supported");

	using pointer = refcnt_ptr<T>;

	fixed_pool() noexcept
	{
		if constexpr (R) {
			// Default-construct all objects for reusable pool
			for (size_t i = 0; i < N; i++) {
				new (&m_objects[i]) T();
			}
		}
	}

	fixed_pool(fixed_pool &&) = delete;
	fixed_pool(const fixed_pool &) = delete;
	fixed_pool &operator=(fixed_pool &&) = delete;
	fixed_pool &operator=(const fixed_pool &) = delete;

	~fixed_pool() noexcept
	{
		// Pool must outlive all allocated objects (otherwise
		// means either a memory leak or a dangling pointer)
		assert(m_used_bitmap.popcount() == 0);
	}

	// Tries to allocate an object from the pool, constructing it with provided
	// arguments. Returns null pointer when no space is left in this pool.
	// If a constructor throws exception, pool's state does not change.
	// This variant of method is applicable to non-reusable pools only.
	// NOTE: this method is thread-safe but is not atomic: allocation may fail even
	// if there is free space (when some other thread has just freed an object).
	template<typename... Args>
		requires(!R)
	pointer allocate(Args &&...args)
	{
		std::lock_guard lock(m_lock);

		size_t pos = m_used_bitmap.occupy_zero();
		if (pos == SIZE_MAX) {
			return pointer();
		}

		T *object = nullptr;
		try {
			object = new (&m_objects[pos]) T(std::forward<Args>(args)...);
		}
		catch (...) {
			m_used_bitmap.clear(pos);
			throw;
		}

		// Relaxed ordering here since the operation doesn't even need
		// to be atomic - unlocking has the needed release semantics
		m_usage_counts[pos].store(1, std::memory_order_relaxed);
		return pointer(object, function_ref(*this));
	}

	// Tries to allocate an object from the pool. Returns null pointer when no space is left in this pool.
	// This variant of method is applicable to reusable pools only.
	// NOTE: this method is thread-safe but is not atomic: allocation may fail
	// even if there is free space (when some other thread has just freed an object).
	template<typename = void>
		requires(R)
	pointer allocate() noexcept
	{
		std::lock_guard lock(m_lock);

		size_t pos = m_used_bitmap.occupy_zero();
		if (pos == SIZE_MAX) {
			return pointer();
		}

		// Relaxed ordering here since the operation doesn't even need
		// to be atomic - unlocking has the needed release semantics
		m_usage_counts[pos].store(1, std::memory_order_relaxed);

		T *object = std::launder(reinterpret_cast<T *>(&m_objects[pos]));
		return pointer(object, function_ref(*this));
	}

	// Returns the number of free objects in the pool. This method is thread-safe.
	// NOTE: this value is only an estimate when using pool from multiple threads.
	uint32_t free_space() noexcept
	{
		std::lock_guard lock(m_lock);
		return N - uint32_t(m_used_bitmap.popcount());
	}

	// Object lifecycle management function, do not call it directly
	void operator()(T *object, refcnt_ptr_action action) noexcept
	{
		assert(object);
		const ptrdiff_t pos_diff = object - reinterpret_cast<T *>(&m_objects[0]);
		assert(pos_diff >= 0);
		const auto id = size_t(pos_diff);

		if (action == refcnt_ptr_action::acquire_ref) {
			// Using relaxed ordering here as increasing refcount does not synchronize with anything
			auto old_count = m_usage_counts[id].fetch_add(1, std::memory_order_relaxed);
			// Check there was no reference count overflow
			assert(old_count < std::numeric_limits<decltype(old_count)>::max());
			(void) old_count; // For builds with disabled asserts
		} else /*(action == refcnt_ptr_action::release_ref)*/ {
			auto old_count = m_usage_counts[id].fetch_sub(1, std::memory_order_release);
			if (old_count != 1) {
				return;
			}

			// This was the last owner, now destroy the object
			if constexpr (!R) {
				object->~T();
			} else {
				static_assert(std::is_nothrow_invocable_v<decltype(&T::clear), T>,
					"Managed object in reusable pool must have `clear() noexcept` method");
				object->clear();
			}

			std::lock_guard lock(m_lock);
			m_used_bitmap.clear(id);
		}
	}

private:
	futex m_lock;
	bitset<N> m_used_bitmap;
	std::array<std::atomic_uint8_t, N> m_usage_counts;
	typename detail::fixed_pool_storage<T, N, R>::type m_objects;
};

template<typename T, size_t N>
using reusable_fixed_pool = fixed_pool<T, N, true>;

} // namespace extras
