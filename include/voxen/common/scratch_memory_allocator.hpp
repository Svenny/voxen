#pragma once

#include <voxen/common/common_fwd.hpp>
#include <voxen/visibility.hpp>

#include <extras/pimpl.hpp>

#include <cstdint>

namespace voxen
{

// Scratch (AKA stack, bump or linear) memory allocator.
// This design is a bit improved over what is usually used and
// supports subscopes to partially reclaim memory, making it
// even more similar to the usual program stack.
//
// Used for allocations that don't leave the scope of a certain
// function, basically an unbounded `alloca()` replacement.
//
// Also see `PipeMemoryAllocator` for fast, short-lived allocations
// that can leave scopes or be freely passed between threads.
//
// This class is NOT thread-safe.
class VOXEN_API ScratchMemoryAllocator {
public:
	ScratchMemoryAllocator() noexcept;
	ScratchMemoryAllocator(ScratchMemoryAllocator &&) = delete;
	ScratchMemoryAllocator(const ScratchMemoryAllocator &) = delete;
	ScratchMemoryAllocator &operator=(ScratchMemoryAllocator &&) = delete;
	ScratchMemoryAllocator &operator=(const ScratchMemoryAllocator &) = delete;
	~ScratchMemoryAllocator();

	// Open the first allocation scope.
	// All allocations are reclaimed (freed) after it closes.
	//
	// When this scope closes (destroys), the allocator becomes fully reset
	// (`allocatedBytes()` will return zeros, `capacityBytes()` might change).
	//
	// Creating a new scope while one is already live (not through
	// its `subscope()` method) will trigger "bug found" immediately.
	ScratchMemoryAllocatorScope scope() noexcept;

	// Estimate the number of bytes currently allocated from
	// this allocator, can be also referred to as "waterline".
	//
	// It is zero when nothing is allocated and is a conservative estimate
	// otherwise - might be larger due to padding and some fragmentation.
	size_t allocatedBytes() const noexcept;

	// Esimate the number of memory bytes currently used by this allocator.
	// It will automatically increase as more memory is requested and
	// can also automatically shrink to reduce unused capacity.
	//
	// It roughly indicates how high `allocatedBytes()` can get before
	// needing more upstream allocations (from global `operator new`),
	// however because of chunking (slabbing) used internally some large and/or
	// over-aligned allocations might end up generating fragmentation (wasted space)
	// that will not be used until those allocations are reclaimed.
	//
	// This number will usually be an underestimate, as there is some
	// overhead coming from the underlying memory allocator (new/malloc).
	//
	// Always greater than or equal to `allocatedBytes()`.
	size_t capacityBytes() const noexcept;

private:
	extras::pimpl<detail::ScratchMemoryAllocatorImpl, 64, 8> m_impl;
};

// An allocation scope of `ScratchMemoryAllocator`. Once it closes,
// all memory allocated from the "parent" allocator (even if through
// another scope object) is reclaimed (freed) and can be allocated again.
//
// If used carefully, scope stack allows to reuse some scratch memory
// between different sub-trees of the call tree within one frame.
//
// This class is NOT thread-safe.
class VOXEN_API ScratchMemoryAllocatorScope {
public:
	explicit ScratchMemoryAllocatorScope(detail::ScratchMemoryAllocatorImpl &alloc, size_t prev_baseline,
		size_t baseline) noexcept;
	ScratchMemoryAllocatorScope(ScratchMemoryAllocatorScope &&) = delete;
	ScratchMemoryAllocatorScope(const ScratchMemoryAllocatorScope &) = delete;
	ScratchMemoryAllocatorScope &operator=(ScratchMemoryAllocatorScope &&) = delete;
	ScratchMemoryAllocatorScope &operator=(const ScratchMemoryAllocatorScope &) = delete;
	~ScratchMemoryAllocatorScope();

	// Allocate `size` bytes aligned to `align` which must be a power of two.
	//
	// There are no particular restrictions on sizes and alignments. The underlying
	// (upstream) allocator is basic global `operator new`, and its failures (`bad_alloc` etc.)
	// will be rethrown as is.
	//
	// Returned pointer MUST NOT be used after this object instance is destroyed.
	[[gnu::malloc, gnu::alloc_size(2), gnu::alloc_align(3), nodiscard]] void *allocate(size_t size,
		size_t align = alignof(std::max_align_t));

	// Allocate and construct an object, semantic equivalent of `new T(args...)`.
	// If the object constructor throws, memory remains used until this scope closes.
	// Otherwise, the caller owns the returned pointer and should destory it if needed.
	template<typename T, typename... Args>
	[[nodiscard]] T *make(Args &&...args)
	{
		void *storage = allocate(sizeof(T), alignof(T));
		return new (storage) T(std::forward<Args>(args)...);
	}

	// Open a subscope. All allocations made after opening it, even if through
	// a different scope object, are reclaimed (freed) when this one closes.
	//
	// When used improperly, this can be a source of hard-to-debug memory corruptions
	// (just like stack corruptions). You are strongly advised NOT to create subscopes
	// unless you are completely sure that no allocation will escape the subscope.
	//
	// Creating a new subscope while one "child" subscope is already live
	// (not through that child object) will trigger "bug found" immediately.
	ScratchMemoryAllocatorScope subscope() noexcept;

private:
	detail::ScratchMemoryAllocatorImpl &m_allocator;
	// `m_baseline` value of the previous ("parent") scope
	size_t m_prev_baseline;
	// "Waterline" at the time of opening this scope
	size_t m_baseline;
};

// See `ScratchMemoryAllocatorScope`.
// Implementing std allocator semantics, usable in containers etc.
// Watch out for subscopes! Any container created within a scope
// must be completely destroyed before closing that scope.
template<typename T>
struct TScratchMemoryAllocator {
	using value_type = T;

	TScratchMemoryAllocator(ScratchMemoryAllocatorScope &alloc) noexcept : alloc(alloc) {}

	template<typename U>
	TScratchMemoryAllocator(const TScratchMemoryAllocator<U> &other) noexcept : alloc(other.alloc)
	{}

	bool operator==(const TScratchMemoryAllocator &) const = default;

	T *allocate(size_t n) { return reinterpret_cast<T *>(alloc.allocate(sizeof(T) * n, alignof(T))); }

	void deallocate(T * /*ptr*/, size_t /*n*/) noexcept {}

	static size_t max_size() noexcept { return SIZE_MAX / sizeof(T); }

	ScratchMemoryAllocatorScope &alloc;
};

} // namespace voxen
