#include <voxen/common/pipe_memory_allocator.hpp>

#include <voxen/os/futex.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <atomic>
#include <cassert>
#include <cstddef>
#include <memory>
#include <new>

namespace voxen
{

namespace
{

struct PipeMemorySlabControl {
	uint32_t allocated_bytes = 0;
	std::atomic_uint32_t live_allocations = 0;
};

// Whole slab size, must be a large power of two to nicely align with hugepages
constexpr size_t SLAB_SIZE = 2u * PipeMemoryAllocator::MAX_ALLOC_SIZE;
// Available storage size, reduced by control data overhead
constexpr size_t STORAGE_SIZE = SLAB_SIZE - sizeof(PipeMemorySlabControl);

struct PipeMemorySlab {
	PipeMemorySlab() = default;
	PipeMemorySlab(PipeMemorySlab&&) = delete;
	PipeMemorySlab(const PipeMemorySlab&) = delete;
	PipeMemorySlab& operator=(PipeMemorySlab&&) = delete;
	PipeMemorySlab& operator=(const PipeMemorySlab&) = delete;
	~PipeMemorySlab() = default;

	void* tryAllocate(size_t size, size_t align) noexcept
	{
		// Points past the last storage byte
		const uintptr_t end = reinterpret_cast<uintptr_t>(this + 1);
		// Allocation top - points past the last free byte
		const uintptr_t top = end - ctl.allocated_bytes;
		// Allocation bottom - points to the lowest free byte
		const uintptr_t bottom = reinterpret_cast<uintptr_t>(storage);
		// Allocated pointer would be here (subtract size, then align)
		const uintptr_t ptr = (top - size) & ~(align - 1u);

		if (ptr >= bottom) [[likely]] {
			// In bounds, enough space for this allocation
			ctl.allocated_bytes = static_cast<uint32_t>(end - ptr);
			// Relaxed - counter ordering is not needed until we move this slab
			// to the garbage list; but then we won't reach this line anymore.
			ctl.live_allocations.fetch_add(1, std::memory_order_relaxed);
			return reinterpret_cast<void*>(ptr);
		}

		// `ptr < bottom` - out of bounds, not enough space
		return nullptr;
	}

	void reset() noexcept
	{
		ctl.allocated_bytes = 0;
		// No need to reset `live_allocations`, must already be zero
	}

	PipeMemorySlabControl ctl;
	// Allocated top-down
	std::byte storage[STORAGE_SIZE];
};

static_assert(sizeof(PipeMemorySlab) == SLAB_SIZE, "Check SLAB_SIZE/STORAGE_SIZE correctness");

PipeMemorySlab* newSlab()
{
	// TODO: map memory with hugepages
	void* ptr = operator new(SLAB_SIZE, std::align_val_t(SLAB_SIZE));
	return new (ptr) PipeMemorySlab;
}

void deleteSlab(PipeMemorySlab* slab) noexcept
{
	// Ensure no live allocations remain
	uint32_t live_allocs = slab->ctl.live_allocations.load(std::memory_order_acquire);
	assert(live_allocs == 0);

	if (live_allocs != 0) [[unlikely]] {
		// TODO: call bugreport function?
		Log::fatal("PipeMemoryAllocator bug: deleting slab ({}, {} allocated bytes) with {} live allocations remaining",
			fmt::ptr(slab), slab->ctl.allocated_bytes, live_allocs);
		Log::fatal("Live allocations remain => your memory is corrupted, buckle up!");
	}

	// Purely formal call, does nothing
	slab->~PipeMemorySlab();
	// TODO: unmap memory (hugepages)
	operator delete(slab, std::align_val_t(SLAB_SIZE));
}

PipeMemoryAllocator::Config g_service_config;

// We might use several instances of this struct
// to distribute lock contention among threads
class SlabCollection {
public:
	SlabCollection() = default;
	SlabCollection(SlabCollection&&) = delete;
	SlabCollection(const SlabCollection&) = delete;
	SlabCollection& operator=(SlabCollection&&) = delete;
	SlabCollection& operator=(const SlabCollection&) = delete;

	~SlabCollection() noexcept
	{
		for (PipeMemorySlab* slab : m_gc_slabs) {
			deleteSlab(slab);
		}

		for (PipeMemorySlab* slab : m_free_slabs) {
			deleteSlab(slab);
		}
	}

	void putGarbageSlab(PipeMemorySlab* slab) noexcept
	{
		std::lock_guard lk(m_lock);
		m_gc_slabs.emplace_back(slab);
	}

	PipeMemorySlab* replaceSlab(PipeMemorySlab* slab)
	{
		// Scoped lock - `newSlab()` is slow and needs no locking
		{
			std::lock_guard lk(m_lock);

			// Should be true any time after the first call from this thread
			if (slab) [[likely]] {
				m_gc_slabs.emplace_back(slab);
				slab = nullptr;
			}

			// We should have at least one free slab ready unless
			// we're at startup or there is an allocation spike.
			if (!m_free_slabs.empty()) [[likely]] {
				slab = m_free_slabs.back();
				m_free_slabs.pop_back();
			}
		}

		return slab ? slab : newSlab();
	}

	void reclaimFreedSlabs() noexcept
	{
		PipeMemorySlab* slab_to_delete = nullptr;

		// Scoped lock - `deleteSlab()` is slow and needs no locking
		{
			std::lock_guard lk(m_lock);

			// Don't keep too many free slabs, they can be allocated after
			// a memory usage spike and will simply waste memory afterwards.
			//
			// TODO: this has an implicit dependency on GC call period.
			// Need to count free slabs over some larger time interval, like 1-2 seconds.
			if (m_free_slabs.size() > g_service_config.destroy_free_slabs_threshold) {
				slab_to_delete = m_free_slabs.back();
				m_free_slabs.pop_back();
			}

			for (size_t i = 0; i < m_gc_slabs.size(); /*nothing*/) {
				PipeMemorySlab* slab = m_gc_slabs[i];

				// Relaxed ordering - we simply need to observe zero at some point.
				// There is no chance of it increasing until we reset the slab.
				if (slab->ctl.live_allocations.load(std::memory_order_relaxed) == 0) {
					slab->reset();

					// Order does not matter, swap with the last one and pop it
					std::swap(m_gc_slabs[i], m_gc_slabs.back());
					m_gc_slabs.pop_back();

					// This slab can now be used for allocations again
					m_free_slabs.emplace_back(slab);
				} else {
					++i;
				}
			}
		}

		if (slab_to_delete) {
			deleteSlab(slab_to_delete);
		}
	}

private:
	os::FutexLock m_lock;
	std::vector<PipeMemorySlab*> m_gc_slabs;
	std::vector<PipeMemorySlab*> m_free_slabs;
};

// Global collection of garbage and free slabs.
// We can add more if lock contention becomes a problem.
SlabCollection g_slab_collection;
// Garbage collection thread, wakes up periodically to reclaim
// deallocated slabs, moving them from garbage to the free list.
// TODO: migrate to job system (as a recurring task) once it's implemented.
std::thread g_slab_gc_thread;
// Set to `true` while GC thread should continue running
std::atomic_bool g_slab_gc_run_flag = false;

void gcThreadProc()
{
	// TODO: we might want to scale this period depending on GC load.
	// When few garbage slabs are submitted we can wake even less often (though
	// it's extremely unlikely that this thread creates any noticeable load).
	// Similarly, when garbage submission is high, we might reduce GC interval
	// to reclaim slabs faster and avoid some excessive memory allocations.
	const auto gc_period = std::chrono::milliseconds(g_service_config.gc_period_msec);

	Log::info("Pipe memory allocator GC thread started");

	while (g_slab_gc_run_flag.load(std::memory_order_relaxed)) {
		std::this_thread::sleep_for(gc_period);
		g_slab_collection.reclaimFreedSlabs();
	}

	Log::info("Pipe memory allocator GC thread stopped");
}

struct ThreadSlabDeleter {
	void operator()(PipeMemorySlab* slab) noexcept
	{
		// This deleter should only get called at thread exit, as otherwise
		// we disown it manually (see `PipeMemoryAllocator::allocate()`).
		//
		// It's very likely that the whole program is about to exit too (no problem
		// even if it's not) so there is no need to reclaim this slab anymore.
		// Try deleting it right here to reduce cleanup workload for the main thread.
		if (slab->ctl.live_allocations.load(std::memory_order_acquire) == 0) {
			// No live allocations, safe to delete
			deleteSlab(slab);
			return;
		}

		// Thread-local destructors are ordered before static ones.
		// Therefore this collection can still be used even after GC thread stops.
		// Remaining slabs will get deleted in its destructor at program exit.
		g_slab_collection.putGarbageSlab(slab);
	}
};

// Thread-local cached slab. Thread owns it exclusively and can allocate without sync.
thread_local std::unique_ptr<PipeMemorySlab, ThreadSlabDeleter> t_this_thread_slab;

} // anonymous namespace

PipeMemoryAllocator::PipeMemoryAllocator(svc::ServiceLocator& /*svc*/, Config cfg)
{
	if (g_slab_gc_run_flag.load(std::memory_order_acquire)) {
		Log::error("PipeMemoryAllocator service is already started!");
		throw Exception::fromError(VoxenErrc::AlreadyRegistered, "PipeMemoryAllocator singleton violated");
	}

	g_service_config = cfg;
	g_slab_gc_run_flag.store(true, std::memory_order_release);
	g_slab_gc_thread = std::thread(gcThreadProc);
}

PipeMemoryAllocator::~PipeMemoryAllocator() noexcept
{
	assert(g_slab_gc_run_flag.load(std::memory_order_acquire));
	assert(g_slab_gc_thread.joinable());

	g_slab_gc_run_flag.store(false, std::memory_order_release);
	g_slab_gc_thread.join();
}

void* PipeMemoryAllocator::allocate(size_t size, size_t align)
{
	if (size > MAX_ALLOC_SIZE || align > MAX_ALIGNMENT) [[unlikely]] {
		// TODO: throw voxen-specific exception (with stacktrace and all)
		throw std::bad_alloc();
	}

	PipeMemorySlab* thread_slab = t_this_thread_slab.get();

	if (thread_slab) [[likely]] {
		void* ptr = thread_slab->tryAllocate(size, align);
		if (ptr) [[likely]] {
			return ptr;
		}

		// Out of free space. For implementation simplicity,
		// we accept a chance of memory waste (the next, smaller
		// allocation could have been served from this slab) and
		// move it to the garbage list.
	}

	// Put this slab into the garbage list and get a new one
	thread_slab = g_slab_collection.replaceSlab(t_this_thread_slab.release());
	t_this_thread_slab.reset(thread_slab);

	// Now this must succeed
	void* ptr = thread_slab->tryAllocate(size, align);
	assert(ptr);
	return ptr;
}

void PipeMemoryAllocator::deallocate(void* ptr) noexcept
{
	if (!ptr) [[unlikely]] {
		return;
	}

	// Simply mask off lower bits to get slab base address
	uintptr_t slab_ptr = uintptr_t(ptr) & ~(SLAB_SIZE - 1u);
	std::launder(reinterpret_cast<PipeMemorySlab*>(slab_ptr))
		->ctl.live_allocations.fetch_sub(1, std::memory_order_release);
}

} // namespace voxen
