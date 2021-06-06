#pragma once

#include <extras/bitset.hpp>
#include <extras/spinlock.hpp>

#include <cassert>
#include <array>
#include <atomic>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>

namespace extras
{

// A thread-safe pool holding up to `N` objects of type `T`. It returns
// reference-counted pointers which will automatically recycle the object.
// WARNING: do not introduce cyclic pointer dependencies. This is not
// manageable by reference counting and will lead to memory leak.
template<typename T, uint32_t N>
class fixed_pool final {
public:
	static_assert(std::is_nothrow_destructible_v<T>, "Managed object must be nothrow destructible");
	static_assert(!std::is_array_v<T>, "Managing C arrays is not supported");

	// Reference-counted pointer to an object allocated from this pool.
	// Reference counter is thread-safe but the pointer itself is not - that is,
	// you can simultaneously have pointers to the same object in multiple threads.
	// Access to the managed object is not synchronized by the pointer.
	// WARNING: only 255 pointers to the same object are allowed to exist
	// simultaneously. Exceeding this limit leads to undefined behavior.
	class ptr final {
	public:
		ptr() = default;

		ptr(ptr &&other) noexcept
		{
			m_object = std::exchange(other.m_object, nullptr);
			m_pool = std::exchange(other.m_pool, nullptr);
		}

		ptr(const ptr &other) noexcept : m_object(other.m_object), m_pool(other.m_pool) { acquire_ref(); }

		ptr &operator = (ptr &&other) noexcept
		{
			std::swap(m_object, other.m_object);
			std::swap(m_pool, other.m_pool);
			return *this;
		}

		ptr &operator = (const ptr &other) noexcept
		{
			if (m_object == other.m_object) {
				return *this;
			}

			release_ref();
			m_object = other.m_object;
			m_pool = other.m_pool;
			acquire_ref();
			return *this;
		}

		~ptr() noexcept { release_ref(); }

		// Release managed object, pointer becomes null
		void reset() noexcept
		{
			release_ref();
			m_object = nullptr;
			m_pool = nullptr;
		}

		T *get() const noexcept { return m_object; }

		explicit operator bool () const noexcept { return m_object != nullptr; }
		std::add_lvalue_reference_t<T> operator * () const noexcept(noexcept(*m_object)) { return *m_object; }
		T *operator -> () const noexcept { return m_object; }

	private:
		ptr(T *object, fixed_pool *pool) noexcept : m_object(object), m_pool(pool) {}

		T *m_object = nullptr;
		fixed_pool *m_pool = nullptr;

		void acquire_ref() noexcept
		{
			if (!m_object) {
				return;
			}
			assert(m_pool);

			const size_t id = m_object - reinterpret_cast<T *>(&m_pool->m_objects[0]);
			// Using relaxed ordering here as increasing refcount does not synchronize with anything
			auto old_count = m_pool->m_usage_counts[id].fetch_add(1, std::memory_order_relaxed);
			// Check there was no reference count overflow
			assert(old_count < std::numeric_limits<decltype(old_count)>::max());
			(void)old_count; // For builds with disabled asserts
		}

		void release_ref() noexcept
		{
			if (!m_object) {
				return;
			}
			assert(m_pool);

			const size_t id = m_object - reinterpret_cast<T *>(&m_pool->m_objects[0]);

			auto old_count = m_pool->m_usage_counts[id].fetch_sub(1, std::memory_order_release);
			if (old_count != 1) {
				return;
			}

			// This was the last owner, now destroy the object
			m_object->~T();

			std::lock_guard<spinlock> lock(m_pool->m_lock);
			m_pool->m_used_bitmap.clear(id);
		}

		friend class fixed_pool;
	};

	fixed_pool() = default;
	fixed_pool(fixed_pool &&) = delete;
	fixed_pool(const fixed_pool &) = delete;
	fixed_pool &operator = (fixed_pool &&) = delete;
	fixed_pool &operator = (const fixed_pool &) = delete;

	~fixed_pool() noexcept
	{
		// Pool must outlive all allocated objects (otherwise
		// means either a memory leak or a dangling pointer)
		assert(m_used_bitmap.popcount() == 0);
	}

	// Tries to allocate an object from the pool, constructing it with provided
	// arguments. Returns null pointer when no space is left in this pool.
	// If a constructor throws exception, pool's state does not change.
	// NOTE: this method is thread-safe but is not atomic: allocation may fail even
	// if there is free space (when some other thread has just freed an object).
	template<typename... Args>
	ptr allocate(Args&&... args)
	{
		std::lock_guard<spinlock> lock(m_lock);

		size_t pos = m_used_bitmap.occupy_zero();
		if (pos == SIZE_MAX) {
			return ptr();
		}

		T *object = nullptr;
		try {
			object = new (&m_objects[pos]) T(std::forward<Args>(args)...);
		} catch(...) {
			m_used_bitmap.clear(pos);
			throw;
		}

		m_usage_counts[pos].store(1, std::memory_order_acquire);
		return ptr(object, this);
	}

	// Returns the number of free objects in the pool. This method is thread-safe.
	// NOTE: this value is only an estimate when using pool from multiple threads.
	uint32_t free_space() noexcept
	{
		std::lock_guard<spinlock> lock(m_lock);
		return N - m_used_bitmap.popcount();
	}

private:
	spinlock m_lock;
	bitset<N> m_used_bitmap;
	std::array<std::atomic_uint8_t, N> m_usage_counts;
	std::array<std::aligned_storage_t<sizeof(T), alignof(T)>, N> m_objects;
};

}
