#pragma once

#include <voxen/visibility.hpp>

#include <cstddef>

namespace voxen
{

// TODO: describe this service
class VOXEN_API PipeMemoryAllocator {
public:
	constexpr static size_t MAX_ALLOC_SIZE = 1024 * 1024;
	constexpr static size_t MAX_ALIGNMENT = 256;

	static void startService();
	static void stopService() noexcept;

	// Allocate `size` bytes of uninitialized storage, aligned to `align` (must be power of two).
	// Will throw `std::bad_alloc()` if `size > MAX_ALLOC_SIZE` or `align > MAX_ALIGNMENT`.
	// Can also throw it if the underlying (upstream) memory allocation fails.
	// This call is fast, the short path is a thread-local access and a bunch of arithmetic.
	// NOTE: you must not call it before the service is started or after it is stopped.
	static void* allocate(size_t size, size_t align);
	// Free pointer returned by previous call to `allocate()` (can be null).
	// This call is EXTREMELY fast, basically just one branch and one atomic op.
	// NOTE: technically this does not deallocate but rather marks one allocation
	// from the memory block as no longer "live". Memory will get actually
	// reclaimed later, once no "live" allocations remain in the whole block.
	// NOTE: you MUST call it for every allocation before the service is stopped.
	static void deallocate(void* ptr) noexcept;

	// Release memory block cached for this thread.
	// Calling this function is not required for correct operation.
	static void dropThreadCache() noexcept;
};

// Implementing std allocator semantics, usable in containers etc.
template<typename T>
struct TPipeMemoryAllocator {
	using value_type = T;

	TPipeMemoryAllocator() = default;
	TPipeMemoryAllocator(TPipeMemoryAllocator&&) = default;
	TPipeMemoryAllocator(const TPipeMemoryAllocator&) = default;
	TPipeMemoryAllocator& operator=(TPipeMemoryAllocator&&) = default;
	TPipeMemoryAllocator& operator=(const TPipeMemoryAllocator&) = default;
	~TPipeMemoryAllocator() = default;

	template<typename U>
	TPipeMemoryAllocator(const TPipeMemoryAllocator<U>&) noexcept
	{}

	bool operator==(const TPipeMemoryAllocator&) const = default;

	T* allocate(size_t n)
	{
		void* ptr = PipeMemoryAllocator::allocate(sizeof(T) * n, alignof(T));
		return reinterpret_cast<T*>(ptr);
	}

	void deallocate(T* ptr, size_t /*n*/) noexcept { return PipeMemoryAllocator::deallocate(ptr); }

	static size_t max_size() noexcept { return PipeMemoryAllocator::MAX_ALLOC_SIZE / sizeof(T); }
};

} // namespace voxen
