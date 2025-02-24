#include <voxen/common/scratch_memory_allocator.hpp>

#include <voxen/debug/bug_found.hpp>

#include <cassert>
#include <memory>
#include <vector>

namespace voxen
{

class detail::ScratchMemoryAllocatorImpl {
public:
	constexpr static size_t INITIAL_SLAB_SIZE = 256 * 1024;
	constexpr static size_t SLAB_SIZE_ALIGNMENT = 16 * 1024;

	ScratchMemoryAllocatorImpl() { m_slabs.emplace_back(INITIAL_SLAB_SIZE, 0); }

	void *allocate(size_t size, size_t align)
	{
		while (m_current_slab_index < m_slabs.size()) {
			Slab &slab = m_slabs[m_current_slab_index];

			void *return_ptr = std::align(align, size, slab.allocation_ptr, slab.remaining_space);
			if (return_ptr) [[likely]] {
				slab.allocation_ptr = reinterpret_cast<std::byte *>(return_ptr) + size;
				slab.remaining_space -= size;
				return return_ptr;
			}

			m_current_slab_index++;
		}

		Slab &last_slab = m_slabs.back();
		size_t new_baseline = last_slab.baseline + last_slab.size;
		// Use at least `INITIAL_SLAB_SIZE` or we might get tiny splinter allocations.
		// XXX: should we also take the last/total slab sizes into account to get some
		// exponential size growth like `std::vector`? Here it's not that important,
		// we should quickly converge to a single slab fitting all allocations for a frame
		// (see fusing/shrinking logic in `closeScope()`).
		size_t new_slab_size = alignSlabSize(std::max(INITIAL_SLAB_SIZE, 2 * size));
		m_slabs.emplace_back(new_slab_size, new_baseline);
		// There will be no further recursion (we've just allocated at least twice the requested size)
		return allocate(size, align);
	}

	size_t openScope(size_t prev_baseline) noexcept
	{
		if (prev_baseline != m_last_scope_baseline) [[unlikely]] {
			// This would be hell to debug if left unnoticed, let's just terminate here
			debug::bugFound("Stack order mismatch when opening scratch allocator scope");
		}

		// Calculate current waterline, this is the baseline for the new scope
		Slab &current_slab = m_slabs[m_current_slab_index];
		size_t waterline = current_slab.baseline + (current_slab.size - current_slab.remaining_space);

		m_last_scope_baseline = waterline;
		return waterline;
	}

	void closeScope(size_t baseline, size_t prev_baseline) noexcept
	{
		if (baseline != std::exchange(m_last_scope_baseline, prev_baseline)) [[unlikely]] {
			// This would be hell to debug if left unnoticed, let's just terminate here
			debug::bugFound("Stack order mismatch when closing scratch allocator scope");
		}

		// Update max waterline
		{
			Slab &current_slab = m_slabs[m_current_slab_index];
			size_t waterline = current_slab.baseline + (current_slab.size - current_slab.remaining_space);
			m_max_waterline = std::max(m_max_waterline, waterline);
		}

		// Reset slabs up to `baseline`, go in reverse order (`~` is `>= 0` for unsigned)
		while (~m_current_slab_index) {
			Slab &slab = m_slabs[m_current_slab_index];

			if (slab.baseline > baseline) {
				// Full reset
				slab.allocation_ptr = slab.memory;
				slab.remaining_space = slab.size;
				// Need to visit the previous slab
				m_current_slab_index--;
			} else {
				// Partial reset, this is the last slab to visit
				slab.allocation_ptr = reinterpret_cast<std::byte *>(slab.memory) + (baseline - slab.baseline);
				slab.remaining_space = slab.size - (baseline - slab.baseline);
				break;
			}
		}

		if (baseline != 0) {
			return;
		}

		// Empty (the first scope is closed), can recreate slabs now

		if (m_slabs.size() > 1) {
			// Several slabs - fuse them to reduce array walking
			size_t fused_size = 0;

			for (Slab &slab : m_slabs) {
				fused_size += slab.size;
			}

			m_slabs.clear();
			m_slabs.emplace_back(alignSlabSize(fused_size), 0);
		} else if (m_slabs[0].size > std::max(INITIAL_SLAB_SIZE, m_max_waterline * 2)) {
			// Fused slab wastes too much space, shrink it (probably after an allocation spike)
			m_slabs.clear();
			// Don't shrink to exactly the waterline to allow for some wiggle.
			// XXX: if per-frame (or whatever is the reset rate) memory usage varies more
			// than this we will have repeated fuses/shrinks for every reset. We could do
			// this part a bit smarter and average "max waterline" over several reset cycles.
			m_slabs.emplace_back(alignSlabSize(m_max_waterline + m_max_waterline / 2), 0);
		}

		// Don't forget to reset max waterline
		m_max_waterline = 0;
	}

	size_t allocatedBytes() const noexcept
	{
		const Slab &current_slab = m_slabs[m_current_slab_index];
		return current_slab.baseline + (current_slab.size - current_slab.remaining_space);
	}

	size_t capacityBytes() const noexcept
	{
		const Slab &last_slab = m_slabs.back();
		return last_slab.baseline + last_slab.size;
	}

private:
	struct Slab {
		explicit Slab(size_t n, size_t b) : size(n), baseline(b), remaining_space(n)
		{
			memory = operator new(n);
			allocation_ptr = memory;
		}

		Slab(Slab &&other) noexcept
			: memory(std::exchange(other.memory, nullptr))
			, size(std::exchange(other.size, 0))
			, baseline(std::exchange(other.baseline, 0))
			, allocation_ptr(std::exchange(other.allocation_ptr, nullptr))
			, remaining_space(std::exchange(other.remaining_space, 0))
		{}

		Slab &operator=(Slab &&other) noexcept
		{
			std::swap(memory, other.memory);
			std::swap(size, other.size);
			std::swap(baseline, other.baseline);
			std::swap(allocation_ptr, other.allocation_ptr);
			std::swap(remaining_space, other.remaining_space);
			return *this;
		}

		Slab(const Slab &) = delete;
		Slab &operator=(const Slab &) = delete;

		~Slab() noexcept { operator delete(memory); }

		void *memory = nullptr;
		size_t size = 0;
		size_t baseline = 0;
		void *allocation_ptr = nullptr;
		size_t remaining_space = 0;
	};

	size_t m_last_scope_baseline = 0;
	size_t m_max_waterline = 0;
	size_t m_current_slab_index = 0;

	std::vector<Slab> m_slabs;

	static size_t alignSlabSize(size_t size) noexcept { return (size + SLAB_SIZE_ALIGNMENT - 1) & ~SLAB_SIZE_ALIGNMENT; }
};

// ScratchMemoryAllocator

ScratchMemoryAllocator::ScratchMemoryAllocator() noexcept = default;
ScratchMemoryAllocator::~ScratchMemoryAllocator() = default;

ScratchMemoryAllocatorScope ScratchMemoryAllocator::scope() noexcept
{
	return ScratchMemoryAllocatorScope(m_impl.object(), 0, m_impl->openScope(0));
}

// ScratchMemoryAllocatorScope

ScratchMemoryAllocatorScope::ScratchMemoryAllocatorScope(detail::ScratchMemoryAllocatorImpl &alloc,
	size_t prev_baseline, size_t baseline) noexcept
	: m_allocator(alloc), m_prev_baseline(prev_baseline), m_baseline(baseline)
{}

ScratchMemoryAllocatorScope::~ScratchMemoryAllocatorScope()
{
	m_allocator.closeScope(m_baseline, m_prev_baseline);
}

void *ScratchMemoryAllocatorScope::allocate(size_t size, size_t align)
{
	return m_allocator.allocate(size, align);
}

ScratchMemoryAllocatorScope ScratchMemoryAllocatorScope::subscope() noexcept
{
	return ScratchMemoryAllocatorScope(m_allocator, m_baseline, m_allocator.openScope(m_baseline));
}

size_t ScratchMemoryAllocator::allocatedBytes() const noexcept
{
	return m_impl->allocatedBytes();
}

size_t ScratchMemoryAllocator::capacityBytes() const noexcept
{
	return m_impl->capacityBytes();
}

} // namespace voxen
