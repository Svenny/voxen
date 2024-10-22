#pragma once

#include <voxen/svc/service_base.hpp>
#include <voxen/visibility.hpp>

#include <cstddef>

namespace voxen
{

// This service provides a very fast, thread-safe memory allocator
// for short-lived small-size objects. Its main purpose is for
// cross-agent (or cross-thread) comminucation in "stream" or "pipe"
// style, hence the name. E.g. to store payloads of queued messages.
//
// Basically, it is an extension of a linear (stack) allocator. Memory
// is suballocated out of slabs (large blocks) by simply advancing the
// "head" pointer. Once a slab is exhausted, it is moved to "garbage list"
// to be recycled later, when no "live" allocations remain in it.
// "Live" allocations are tracked by simply counting the numbers
// of `allocate()` and `deallocate()` calls served from a slab.
//
// Individual `allocate()/deallocate()` calls are not matched
// with each other, so memory leaks, double-frees etc. blow up
// the entire service (pretty much like they do with `malloc/free`).
//
// NOTE: do NOT use this allocator for "long-time" allocations,
// i.e. those for which a finite lifetime is not guaranteed.
// Though such allocations are possible (i.e. there are no UBs,
// memory leaks or whatever), they will have insane space overhead.
//
// NOTE: this service is technically a singleton,
// for it employs global state for maximum performance.
class VOXEN_API PipeMemoryAllocator final : public svc::IService {
public:
	constexpr static UID SERVICE_UID = UID("6208542f-29928272-d52e602f-f48b908d");

	constexpr static size_t MAX_ALLOC_SIZE = 1024 * 1024;
	constexpr static size_t MAX_ALIGNMENT = 256;

	struct Config {
		// Service starts an auxiliary garbage collection (GC) thread that
		// periodically checks the "garbage list" and reclaims slabs which
		// have no remaining "live" allocations.
		// This setting controls how often (in milliseconds) this thread wakes up.
		// This thread should have no measurable impact on performance.
		uint32_t gc_period_msec = 20;
		// If GC thread detects that some slabs remain free (untaken) for a long time
		// if will start destroying them until their number is within this threshold.
		// This brings memory consumption back down after an allocation spike.
		uint32_t destroy_free_slabs_threshold = 5;
	};

	// Service constructor, call only from the factory function.
	// If another service instance is currently active,
	// throws `Exception` with `VoxenErrc::AlreadyRegistered`.
	explicit PipeMemoryAllocator(svc::ServiceLocator& svc, Config cfg);
	PipeMemoryAllocator(PipeMemoryAllocator&&) = delete;
	PipeMemoryAllocator(const PipeMemoryAllocator&) = delete;
	PipeMemoryAllocator& operator=(PipeMemoryAllocator&&) = delete;
	PipeMemoryAllocator& operator=(const PipeMemoryAllocator&) = delete;
	~PipeMemoryAllocator() noexcept override;

	UID serviceUid() const noexcept override { return SERVICE_UID; }

	// Allocate `size` bytes of uninitialized storage, aligned to `align` (must be power of two).
	// Will throw `std::bad_alloc()` if `size > MAX_ALLOC_SIZE` or `align > MAX_ALIGNMENT`.
	// Can also throw it if the underlying (upstream) memory allocation fails.
	// This call is fast, the short path is a thread-local access and a bunch of arithmetic.
	//
	// NOTE: you must not call it while the service is not active.
	[[gnu::malloc, gnu::alloc_size(1), gnu::alloc_align(2), nodiscard]] static void* allocate(size_t size, size_t align);

	// Free pointer returned by previous call to `allocate()` (can be null).
	// This call is EXTREMELY fast, basically just one branch and one atomic op.
	// NOTE: technically this does not deallocate but rather marks one allocation
	// from the memory block as no longer "live". Memory will get actually
	// reclaimed later, once no "live" allocations remain in the whole block.
	//
	// NOTE: every allocation must be deallocated *before* the service is stopped.
	static void deallocate(void* ptr) noexcept;
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
