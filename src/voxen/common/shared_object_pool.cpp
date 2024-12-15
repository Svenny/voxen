#include <voxen/common/shared_object_pool.hpp>

#include <voxen/debug/bug_found.hpp>
#include <voxen/util/log.hpp>

#include <cassert>

namespace voxen::detail
{

using RefCounterType = std::atomic_uint16_t;

static_assert(sizeof(RefCounterType) == 2, "Atomic uint16 is not two bytes, wtf?");
static_assert(alignof(RefCounterType) == 2, "Atomic uint16 is over-aligned, rewrite this code");

namespace
{

constexpr static uint32_t REF_COUNTER_SIZE = sizeof(RefCounterType);

struct SlabHeader {
	SharedObjectPoolBase *pool = nullptr;
	void *next_slab = nullptr;
	uint32_t initial_objects = 0;
	std::atomic_uint32_t live_allocations = 0;
};

static_assert(sizeof(SlabHeader) == SharedObjectPoolBase::SLAB_HEADER_SIZE,
	"Update SharedObjectPoolBase::SLAB_HEADER_SIZE");
// Header start alignment in `calcSlabSize()` depends on this
static_assert(alignof(SlabHeader) <= sizeof(void *));

SlabHeader *getSlabHeader(void *slab_base, size_t slab_size) noexcept
{
	std::byte *addr = reinterpret_cast<std::byte *>(slab_base);
	return reinterpret_cast<SlabHeader *>(addr + slab_size - sizeof(SlabHeader));
}

SlabHeader *getObjectSlabHeader(void *obj, size_t slab_size) noexcept
{
	// Simply mask off lower bits to get slab base address
	uintptr_t slab_base = uintptr_t(obj) & ~(slab_size - 1u);
	return reinterpret_cast<SlabHeader *>(slab_base + slab_size - sizeof(SlabHeader));
}

RefCounterType *getRefCounter(void *obj, size_t slab_size, size_t adjusted_object_size) noexcept
{
	uintptr_t obj_addr = reinterpret_cast<uintptr_t>(obj);
	uintptr_t slab_base = obj_addr & ~(slab_size - 1u);
	uintptr_t index = (obj_addr - slab_base) / adjusted_object_size;
	return reinterpret_cast<RefCounterType *>(
		slab_base + slab_size - sizeof(SlabHeader) - REF_COUNTER_SIZE * (index + 1));
}

RefCounterType *getRefCounter(SlabHeader *hdr, uint32_t object_index) noexcept
{
	return reinterpret_cast<RefCounterType *>(reinterpret_cast<uintptr_t>(hdr) - REF_COUNTER_SIZE * (object_index + 1));
}

uint32_t calcMaxObjects(uint32_t adjusted_object_size, uint32_t slab_size) noexcept
{
	return (slab_size - sizeof(SlabHeader)) / (adjusted_object_size + REF_COUNTER_SIZE);
}

} // namespace

SharedObjectPoolBase::SharedObjectPoolBase(size_t object_size, size_t objects_hint) noexcept
	: m_adjusted_object_size(static_cast<uint32_t>(adjustObjectSize(object_size)))
	, m_slab_size(static_cast<uint32_t>(calcSlabSize(object_size, objects_hint)))
	, m_max_objects(calcMaxObjects(m_adjusted_object_size, m_slab_size))
{}

SharedObjectPoolBase::~SharedObjectPoolBase()
{
	void *slab = m_newest_slab;

	while (slab) {
		SlabHeader *hdr = getSlabHeader(slab, m_slab_size);

		uint32_t live_allocs = hdr->live_allocations.load(std::memory_order_acquire);
		assert(live_allocs == 0);

		if (live_allocs > 0) [[unlikely]] {
			// TODO: call bugreport function?
			Log::fatal(
				"SharedObjectPool bug: pool ({}x{} byte objs, {} bytes slab) "
				"destroying slab {} with {} live objects remaining",
				m_max_objects, m_adjusted_object_size, m_slab_size, fmt::ptr(slab), live_allocs);
			Log::fatal("Live objects remain => your memory is corrupted, buckle up!");
		}

		void *next = hdr->next_slab;
		operator delete(slab, std::align_val_t(m_slab_size));
		slab = next;
	}
}

void SharedObjectPoolBase::addRef(void *obj, size_t slab_size, size_t adjusted_object_size) noexcept
{
	constexpr auto COUNTER_MAX = std::numeric_limits<RefCounterType::value_type>::max();

	auto *cnt = getRefCounter(obj, slab_size, adjusted_object_size);
	if (cnt->fetch_add(1, std::memory_order_relaxed) == COUNTER_MAX) [[unlikely]] {
		// This should never happen (how could one have so many refs to a pooled object?)
		// but if it does, we better crash immediately - debugging what happens after will be hell.
		debug::bugFound("SharedObjectPool refcount has overflown uint16 counter!");
	}
}

bool SharedObjectPoolBase::releaseRef(void *obj, size_t slab_size, size_t adjusted_object_size) noexcept
{
	auto *cnt = getRefCounter(obj, slab_size, adjusted_object_size);
	return cnt->fetch_sub(1, std::memory_order_acq_rel) == 1;
}

void SharedObjectPoolBase::deallocate(void *obj, size_t slab_size) noexcept
{
	SlabHeader *hdr = getObjectSlabHeader(obj, slab_size);
	hdr->live_allocations.fetch_sub(1, std::memory_order_release);

	// Make this object the last freed, store the previous pointer into it
	void **p_next_freed_object = reinterpret_cast<void **>(obj);
	SharedObjectPoolBase *pool = hdr->pool;
	void *next_freed_object = pool->m_last_freed_object.load(std::memory_order_acquire);

	// Repeat until CAS succeeds. This is a lock-free concurrent stack push operation.
	// Here ABA is not a problem - missed push-pop of another object will not break forward link.
	do {
		*p_next_freed_object = next_freed_object;
	} while (!pool->m_last_freed_object.compare_exchange_weak(next_freed_object, obj, std::memory_order_release,
		std::memory_order_acquire));
}

void *SharedObjectPoolBase::allocate()
{
	void *last_freed_object = m_last_freed_object.load(std::memory_order_acquire);

	while (last_freed_object != nullptr) {
		// Reuse the last freed object, replace it with the next pointer stored inside it.
		// Note - other threads can't simultaneously allocate (pool is single-threaded)
		// but can deallocate, updating `m_last_freed_object` concurrently.
		// So we might load a stale (no longer "next") value from here.
		// Therefore we need CAS loop to ensure we did load the latest value,
		// otherwise part of free list will get orphaned, causing undetectable memory leak.
		void *next_freed_object = *reinterpret_cast<void **>(last_freed_object);

		// Repeat until CAS succeeds. This is a lock-free concurrent stack pop operation.
		// ABA can't happen here - before `last_freed_object` is allocated (returned from this function,
		// which is required to be single-threaded) it can not appear on the stack again.
		if (m_last_freed_object.compare_exchange_weak(last_freed_object, next_freed_object, std::memory_order_release,
				 std::memory_order_acquire)) [[likely]] {
			SlabHeader *hdr = getObjectSlabHeader(last_freed_object, m_slab_size);
			hdr->live_allocations.fetch_add(1, std::memory_order_relaxed);

			auto *cnt = getRefCounter(last_freed_object, m_slab_size, m_adjusted_object_size);
			cnt->store(1, std::memory_order_release);

			return last_freed_object;
		}
	}

	// Reuse failed, allocate a new entry.
	// This section is fully single-threaded.

	if (!m_newest_slab || getSlabHeader(m_newest_slab, m_slab_size)->initial_objects == m_max_objects) [[unlikely]] {
		void *slab = operator new(m_slab_size, std::align_val_t(m_slab_size));

		SlabHeader *hdr = new (getSlabHeader(slab, m_slab_size)) SlabHeader;
		hdr->pool = this;
		hdr->next_slab = std::exchange(m_newest_slab, slab);
	}

	SlabHeader *hdr = getSlabHeader(m_newest_slab, m_slab_size);
	uint32_t index = hdr->initial_objects++;
	hdr->live_allocations.fetch_add(1, std::memory_order_relaxed);

	auto *cnt = getRefCounter(hdr, index);
	cnt->store(1, std::memory_order_release);

	return reinterpret_cast<std::byte *>(m_newest_slab) + index * m_adjusted_object_size;
}

} // namespace voxen::detail
