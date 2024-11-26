#include <voxen/common/private_object_pool.hpp>

#include <voxen/util/log.hpp>

#include <cassert>

namespace voxen::detail
{

namespace
{

struct SlabHeader {
	PrivateObjectPoolBase *pool = nullptr;
	void *next_slab = nullptr;
	uint32_t initial_objects = 0;
	uint32_t _unused = 0;
};

static_assert(sizeof(SlabHeader) == PrivateObjectPoolBase::SLAB_HEADER_SIZE,
	"Update PrivateObjectPoolBase::SLAB_HEADER_SIZE");
// Header start alignment in `calcSlabSize()` depends on this
static_assert(alignof(SlabHeader) <= sizeof(void *));

SlabHeader *getSlabHeader(void *slab_base, size_t slab_size) noexcept
{
	std::byte *addr = reinterpret_cast<std::byte *>(slab_base);
	return reinterpret_cast<SlabHeader *>(addr + slab_size - sizeof(SlabHeader));
}

uint32_t calcMaxObjects(uint32_t adjusted_object_size, uint32_t slab_size) noexcept
{
	return (slab_size - sizeof(SlabHeader)) / adjusted_object_size;
}

} // namespace

PrivateObjectPoolBase::PrivateObjectPoolBase(size_t object_size, size_t objects_hint) noexcept
	: m_adjusted_object_size(static_cast<uint32_t>(adjustObjectSize(object_size)))
	, m_slab_size(static_cast<uint32_t>(calcSlabSize(object_size, objects_hint)))
	, m_max_objects(calcMaxObjects(m_adjusted_object_size, m_slab_size))
{}

PrivateObjectPoolBase::~PrivateObjectPoolBase()
{
	assert(m_live_allocations == 0);

	if (m_live_allocations > 0) [[unlikely]] {
		// TODO: call bugreport function?
		Log::fatal(
			"PrivateObjectPool bug: pool ({}x{} byte objs, {} bytes slab) "
			"destroying with {} live objects remaining",
			m_max_objects, m_adjusted_object_size, m_slab_size, m_live_allocations);
		Log::fatal("Live objects remain => your memory is corrupted, buckle up!");
	}

	void *slab = m_newest_slab;

	while (slab) {
		void *next = getSlabHeader(slab, m_slab_size)->next_slab;
		operator delete(slab, std::align_val_t(m_slab_size));
		slab = next;
	}
}

void *PrivateObjectPoolBase::allocate()
{
	if (m_last_freed_object != nullptr) {
		m_live_allocations++;
		// Reuse the last freed object, replace it with the next pointer stored inside it
		void **p_next_freed_object = reinterpret_cast<void **>(m_last_freed_object);
		return std::exchange(m_last_freed_object, *p_next_freed_object);
	}

	if (!m_newest_slab || getSlabHeader(m_newest_slab, m_slab_size)->initial_objects == m_max_objects) [[unlikely]] {
		void *slab = operator new(m_slab_size, std::align_val_t(m_slab_size));

		SlabHeader *hdr = new (getSlabHeader(slab, m_slab_size)) SlabHeader;
		hdr->pool = this;
		hdr->next_slab = std::exchange(m_newest_slab, slab);
	}

	m_live_allocations++;
	uint32_t index = getSlabHeader(m_newest_slab, m_slab_size)->initial_objects++;
	return reinterpret_cast<std::byte *>(m_newest_slab) + index * m_adjusted_object_size;
}

void PrivateObjectPoolBase::deallocate(void *obj, size_t slab_size) noexcept
{
	// Simply mask off lower bits to get slab base address
	uintptr_t slab_base = uintptr_t(obj) & ~(slab_size - 1u);
	SlabHeader *hdr = getSlabHeader(reinterpret_cast<void *>(slab_base), slab_size);

	hdr->pool->m_live_allocations--;
	// Make this object the last freed, store the previous pointer into it
	void **p_next_freed_object = reinterpret_cast<void **>(obj);
	*p_next_freed_object = std::exchange(hdr->pool->m_last_freed_object, obj);
}

} // namespace voxen::detail
