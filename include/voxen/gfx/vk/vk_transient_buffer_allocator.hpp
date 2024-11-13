#pragma once

#include <voxen/gfx/frame_tick_id.hpp>
#include <voxen/gfx/gfx_fwd.hpp>
#include <voxen/gfx/vk/vk_include.hpp>

#include <list>

namespace voxen::gfx::vk
{

// Fast path allocator for single-frame GPU buffers.
// Allocations are done in "stream" fashion similar to how `PipeMemoryAllocator`
// allocates CPU memory. Returned buffer handles need no manual lifetime
// management and can be used only during the current frame tick ID.
//
// This is created to serve the most common transient allocation use cases
// with the best performance and minimal (mostly zero) memory fragmentation.
// If your use case is not covered you can still do one-off direct VMA
// allocations and enqueue their frees through `Device` destroy queue.
// That's fine as long as it only happens occasionally.
//
// Allocations are not aliased with anything else.
// All buffers are created with `VK_SHARING_MODE_CONCURRENT` and can be
// used from any device queue. That's because buffer queue sharing mode
// is ignored by every GPU driver I know, so let's not complicate things.
//
// This class is NOT thread-safe.
class TransientBufferAllocator {
public:
	struct Allocation {
		// Vulkan handle of the buffer. Do not destroy it or access
		// outside of the range `[buffer_offset; buffer_offset+size)`.
		VkBuffer buffer = VK_NULL_HANDLE;
		// Offset (bytes) from the beginning of the buffer.
		// Do not add it to `host_pointer`, use only in Vulkan commands.
		VkDeviceSize buffer_offset = 0;
		// CPU mapped pointer to the allocation, null if type is not CPU-visible.
		// Already offset properly, do not add `buffer_offset` to it.
		void *host_pointer = nullptr;
		// Allocation size (bytes), at least as large as the requested size.
		VkDeviceSize size = 0;
	};

	enum Type : uint32_t {
		// Buffer for one-frame usage on GPU. This is intended for variable-size buffers,
		// those with known fixed size should be created as render graph resources or similar.
		//
		// Not CPU-visible, has the following usage flags:
		// - VK_BUFFER_USAGE_TRANSFER_SRC_BIT
		// - VK_BUFFER_USAGE_TRANSFER_DST_BIT
		// - VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
		// - VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		// - VK_BUFFER_USAGE_INDEX_BUFFER_BIT
		// - VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
		// - VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
		//
		// On dGPU systems it is likely to be in VRAM so GPU access should be fast.
		//
		// Usage examples:
		// - Indirect command storage for frustum culling output
		// - Storage for meshlet culling output
		// - Skinned vertices storage if doing pre-skinning
		// - Generally, any storage depending on the number or sizes of drawn objects
		TypeScratch,
		// Same as `TypeScratch` but can be initialized (uploaded) from CPU.
		// Has `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT` and `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT`
		// in addition to everything `TypeScratch` has, i.e. it is CPU-visible.
		//
		// Usage examples:
		// - Staging data for general CPU->GPU uploads if the destination is not host-accessible
		//   (non-linear tiled images, VRAM resources on dGPUs without ReBAR etc.)
		// - Small non-reusable constant buffers (per frame, per draw, per pass, ...)
		// - "Immediate" mesh data for debug draws (small vertex/index buffers)
		// - UI render data that is regenerated every frame (text, vertex/index buffers etc.)
		//
		// Note that on dGPU systems it is unlikely to be in VRAM, making GPU access slower.
		// Ideally you should either read this buffer at most a few times or make it as small as possible.
		// Otherwise filling it from CPU and then copying to `TypeScratch` might be more efficient.
		TypeUpload,

		// Do not use, only for counting buffer types
		TypeCount,
	};

	explicit TransientBufferAllocator(Device &dev);
	TransientBufferAllocator(TransientBufferAllocator &&) = delete;
	TransientBufferAllocator(const TransientBufferAllocator &) = delete;
	TransientBufferAllocator &operator=(TransientBufferAllocator &&) = delete;
	TransientBufferAllocator &operator=(const TransientBufferAllocator &) = delete;
	~TransientBufferAllocator();

	// NOTE: `align` is respective to the underlying buffer start, not its underlying device memory.
	// Buffer start itself is aligned according to features declared in `Type` enum description.
	// However, Vulkan expresses most alignment requirements as buffer offsets so this shouldn't be an issue.
	Allocation allocate(Type type, VkDeviceSize size, VkDeviceSize align);

	void onFrameTickBegin(FrameTickId completed_tick, FrameTickId new_tick);
	void onFrameTickEnd(FrameTickId current_tick);

private:
	struct Buffer;

	Device &m_dev;
	FrameTickId m_current_tick_id = FrameTickId::INVALID;

	std::list<Buffer> m_free_list[TypeCount];
	std::list<Buffer> m_used_list[TypeCount];

	VkDeviceSize m_current_tick_allocated_bytes[TypeCount] = {};
	VkDeviceSize m_allocation_exp_average[TypeCount] = {};

	void addBuffer(Type type, VkDeviceSize min_size);
};

} // namespace voxen::gfx::vk
